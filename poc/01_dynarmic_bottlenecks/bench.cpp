// Synthetic ARM64 benchmark fed directly to dynarmic.
//
// No ROM. No HLE. No game. A tight loop of ADD instructions is placed at a
// fixed guest address and executed repeatedly through Dynarmic::A64::Jit.
// The run is instrumented for the three known dynarmic bottlenecks:
//
//   1. JIT state struct size vs. a 64-byte cache line  (cache-miss proxy)
//   2. Block eviction frequency under cache invalidation
//   3. Prologue / epilogue cycles charged per Run() call
//
// Build: see CMakeLists.txt. Requires a local dynarmic install (the library
// plus its deps: boost, mcl, fmt, xbyak).

#include <dynarmic/interface/A64/a64.h>
#include <dynarmic/interface/A64/config.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

namespace
{

    constexpr std::uint64_t GUEST_ENTRY = 0x1000;
    constexpr std::size_t GUEST_RAM = 1u << 20; // 1 MiB
    constexpr std::size_t CACHE_LINE = 64;      // x86_64 L1 line
    constexpr int OUTER_ITERS = 1'000'000;
    constexpr int EVICT_ITERS = 10'000;

    // 16 ARM64 ADD instructions followed by a RET. This is the hot block the
    // JIT will translate once and execute OUTER_ITERS times. The loop is
    // driven from the host side (Run() -> RET -> Run() -> ...) so the host
    // pays the prologue / epilogue cost on every iteration - exactly the
    // cost we want to measure.
    //
    //   ADD X0, X0, #1     0x91000400
    //   ...
    //   RET                0xD65F03C0
    alignas(8) const std::uint32_t HOT_BLOCK[] = {
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0x91000400,
        0xD65F03C0,
    };

    struct Env final : public Dynarmic::A64::UserCallbacks
    {
        std::array<std::uint8_t, GUEST_RAM> ram{};
        std::uint64_t ticks_left = 0;
        std::uint64_t blocks_executed = 0;

        std::optional<std::uint32_t> MemoryReadCode(std::uint64_t vaddr) override
        {
            return MemoryRead32(vaddr);
        }
        std::uint8_t MemoryRead8(std::uint64_t a) override { return load<std::uint8_t>(a); }
        std::uint16_t MemoryRead16(std::uint64_t a) override { return load<std::uint16_t>(a); }
        std::uint32_t MemoryRead32(std::uint64_t a) override { return load<std::uint32_t>(a); }
        std::uint64_t MemoryRead64(std::uint64_t a) override { return load<std::uint64_t>(a); }
        Dynarmic::A64::Vector MemoryRead128(std::uint64_t a) override
        {
            return {load<std::uint64_t>(a), load<std::uint64_t>(a + 8)};
        }
        void MemoryWrite8(std::uint64_t a, std::uint8_t v) override { store(a, v); }
        void MemoryWrite16(std::uint64_t a, std::uint16_t v) override { store(a, v); }
        void MemoryWrite32(std::uint64_t a, std::uint32_t v) override { store(a, v); }
        void MemoryWrite64(std::uint64_t a, std::uint64_t v) override { store(a, v); }
        void MemoryWrite128(std::uint64_t a, Dynarmic::A64::Vector v) override
        {
            store(a, v[0]);
            store(a + 8, v[1]);
        }

        void InterpreterFallback(std::uint64_t, std::size_t) override {}
        void CallSVC(std::uint32_t) override {}
        void ExceptionRaised(std::uint64_t, Dynarmic::A64::Exception) override {}
        void AddTicks(std::uint64_t t) override { ticks_left -= std::min(t, ticks_left); }
        std::uint64_t GetTicksRemaining() override { return ticks_left; }
        std::uint64_t GetCNTPCT() override { return 0; }

        template <typename T>
        T load(std::uint64_t addr) const
        {
            T v{};
            std::memcpy(&v, &ram[addr], sizeof(T));
            return v;
        }
        template <typename T>
        void store(std::uint64_t addr, T v)
        {
            std::memcpy(&ram[addr], &v, sizeof(T));
        }
    };

    inline std::uint64_t rdtsc()
    {
#if defined(__x86_64__)
        unsigned lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<std::uint64_t>(hi) << 32) | lo;
#else
        return std::chrono::steady_clock::now().time_since_epoch().count();
#endif
    }

    void install_hot_block(Env &env)
    {
        std::memcpy(&env.ram[GUEST_ENTRY], HOT_BLOCK, sizeof(HOT_BLOCK));
    }

} // namespace

int main()
{
    Env env;
    install_hot_block(env);

    Dynarmic::A64::UserConfig cfg;
    cfg.callbacks = &env;

    Dynarmic::A64::Jit jit(cfg);

    //  1. JIT state footprint
    //
    // Dynarmic's Jit holds pointers to a heap-allocated block cache, a
    // backend context, and numerous std::unordered_map instances. The
    // Jit object itself is the struct touched on every Run() entry; if
    // it spans multiple cache lines, each entry costs >=1 additional L1
    // miss on cold invocation.
    const std::size_t jit_bytes = sizeof(Dynarmic::A64::Jit);
    const std::size_t jit_lines = (jit_bytes + CACHE_LINE - 1) / CACHE_LINE;

    std::printf("=== 1. JIT state footprint ===\n");
    std::printf("  sizeof(Dynarmic::A64::Jit) = %zu bytes\n", jit_bytes);
    std::printf("  cache lines spanned        = %zu (target: 1)\n", jit_lines);
    std::printf("  cache-line overhead factor = %.2fx\n\n",
                static_cast<double>(jit_lines));

    //  2. Prologue / epilogue overhead per Run()
    //
    // Each Run() re-enters the JIT dispatcher: save callee-saved regs,
    // reload guest register file into host regs, jump to block, on block
    // exit reverse it. We measure by running the hot block OUTER_ITERS
    // times and dividing.
    jit.SetPC(GUEST_ENTRY);
    for (int i = 0; i < 31; ++i)
        jit.SetRegister(i, 0);
    env.ticks_left = 1;
    jit.Run(); // warm the block cache - first run compiles
    env.blocks_executed = 0;

    const std::uint64_t t0 = rdtsc();
    for (int i = 0; i < OUTER_ITERS; ++i)
    {
        jit.SetPC(GUEST_ENTRY);
        env.ticks_left = 17; // 16 ADDs + RET
        jit.Run();
        env.blocks_executed++;
    }
    const std::uint64_t t1 = rdtsc();

    const double cycles_per_run = static_cast<double>(t1 - t0) / OUTER_ITERS;
    const double cycles_per_arm = cycles_per_run / 17.0;

    std::printf("=== 2. Prologue / epilogue per Run() ===\n");
    std::printf("  total cycles              = %llu\n",
                static_cast<unsigned long long>(t1 - t0));
    std::printf("  Run() calls               = %d\n", OUTER_ITERS);
    std::printf("  cycles per Run()          = %.2f\n", cycles_per_run);
    std::printf("  cycles per ARM insn       = %.2f (floor: ~1.0)\n",
                cycles_per_arm);
    std::printf("  prologue/epilogue tax     = %.0f cycles/entry\n\n",
                cycles_per_run - 17.0);

    //  3. Block eviction frequency
    //
    // Games with self-modifying code or JIT patching trigger
    // InvalidateCacheRange. Dynarmic's block cache is an intrusive linked
    // list; invalidation walks pointers and re-traces every block on
    // next entry. We simulate this by invalidating between iterations.
    jit.ClearCache();
    jit.SetPC(GUEST_ENTRY);
    for (int i = 0; i < 31; ++i)
        jit.SetRegister(i, 0);
    env.ticks_left = 17;
    jit.Run(); // compile

    const std::uint64_t e0 = rdtsc();
    for (int i = 0; i < EVICT_ITERS; ++i)
    {
        jit.InvalidateCacheRange(GUEST_ENTRY, sizeof(HOT_BLOCK));
        jit.SetPC(GUEST_ENTRY);
        env.ticks_left = 17;
        jit.Run(); // forced re-trace
    }
    const std::uint64_t e1 = rdtsc();

    const double cycles_per_evict = static_cast<double>(e1 - e0) / EVICT_ITERS;

    std::printf("=== 3. Block eviction cost ===\n");
    std::printf("  invalidate+Run cycles     = %.0f per iter\n",
                cycles_per_evict);
    std::printf("  vs. cached Run()          = %.1fx slower\n",
                cycles_per_evict / cycles_per_run);
    std::printf("  (proxy for SMC, JIT-patched code, page remap churn)\n\n");

    std::printf("=== Summary ===\n");
    std::printf("  The three numbers Ballistic's design targets directly:\n");
    std::printf("    jit state lines:     %zu   -> 1 in Ballistic\n", jit_lines);
    std::printf("    entry tax cycles:    %.0f  -> <10 in Ballistic\n",
                cycles_per_run - 17.0);
    std::printf("    eviction slowdown:   %.1fx -> ~1x in Ballistic\n",
                cycles_per_evict / cycles_per_run);

    return 0;
}
