/*
 * ballistic_stub.c — the exact surface Ballistic has to fill in.
 *
 * This compiles. It links. It is structurally identical to
 * dynarmic_backend.c. Every function is a stub that does nothing useful;
 * each is annotated with what Ballistic must provide to make it real.
 *
 * The point of this file is to make the contract concrete: there is no
 * mystery about what the rest of the emulator wants from Ballistic, and
 * there is no risk of API drift. Implement these, replace
 * CPU_BACKEND_DYNARMIC with CPU_BACKEND_BALLISTIC at the call site, the
 * rest of the project does not change.
 */

#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The state Ballistic owns per emulated core. The exact contents are an
 * internal Ballistic concern. The host only ever sees CPU_State*. */
typedef struct
{
    void *guest_ram;
    uint64_t ram_size;
    void *userdata;

    /* Mirror minimal arch state so the stub can be exercised end-to-end
     * without a real backend. Ballistic will own the real register file
     * inside its packed JIT-state struct (≤64 bytes — see DESIGN.MD §7). */
    uint64_t x[31];
    uint64_t pc;
    uint64_t sp;
    uint32_t pstate;
    bool halted;

    CPU_SVC_Handler on_svc;
    CPU_Undefined_Handler on_undefined;
    CPU_Breakpoint_Handler on_breakpoint;
} BalState;

/*  Lifecycle  */

static CPU_State *b_create(void *guest_ram, uint64_t ram_size, void *userdata)
{
    /* TODO(Ballistic): allocate the packed JIT state struct (one cache
     * line), prime the block cache, register the executable-memory
     * backend selected at build time (mmap / VirtualAllocFromApp /
     * WASM bytecode buffer). */
    BalState *s = (BalState *)calloc(1, sizeof(BalState));
    if (!s)
        return NULL;
    s->guest_ram = guest_ram;
    s->ram_size = ram_size;
    s->userdata = userdata;
    return (CPU_State *)s;
}

static void b_destroy(CPU_State *state)
{
    /* TODO(Ballistic): tear down block cache, release executable pages. */
    free(state);
}

/*  Execution  */

static void b_run(CPU_State *state, uint64_t entry_point)
{
    /* TODO(Ballistic): main dispatch loop.
     *   1. Look up block at entry_point in cache.
     *   2. If miss: decode ARM -> IR (CFG-preserving, SSA values).
     *   3. Run IR through the platform backend (x64 / arm64 / WASM).
     *   4. Enter compiled block; loop until halt or exit-to-host. */
    BalState *s = (BalState *)state;
    s->pc = entry_point;
    fprintf(stderr,
            "[ballistic-stub] run(pc=0x%016llx) — backend not implemented\n",
            (unsigned long long)entry_point);
}

static void b_step(CPU_State *state)
{
    /* TODO(Ballistic): execute exactly one ARM instruction at PC. */
    (void)state;
}

static void b_run_until(CPU_State *state, uint64_t address)
{
    /* TODO(Ballistic): run() with an extra termination predicate. */
    (void)state;
    (void)address;
}

/*  Register access  */

static uint64_t b_get_reg(CPU_State *state, uint8_t i)
{
    if (i >= 31)
        return 0; /* XZR */
    return ((BalState *)state)->x[i];
}
static void b_set_reg(CPU_State *state, uint8_t i, uint64_t v)
{
    if (i >= 31)
        return; /* XZR writes discarded */
    ((BalState *)state)->x[i] = v;
}

static uint64_t b_get_pc(CPU_State *s) { return ((BalState *)s)->pc; }
static void b_set_pc(CPU_State *s, uint64_t v) { ((BalState *)s)->pc = v; }
static uint64_t b_get_sp(CPU_State *s) { return ((BalState *)s)->sp; }
static void b_set_sp(CPU_State *s, uint64_t v) { ((BalState *)s)->sp = v; }
static uint32_t b_get_pstate(CPU_State *s) { return ((BalState *)s)->pstate; }
static void b_set_pstate(CPU_State *s, uint32_t v) { ((BalState *)s)->pstate = v; }

static uint64_t b_get_sys_reg(CPU_State *s, uint32_t r)
{
    /* TODO(Ballistic): MRS — decode `r` (op0/op1/CRn/CRm/op2). */
    (void)s;
    (void)r;
    return 0;
}
static void b_set_sys_reg(CPU_State *s, uint32_t r, uint64_t v)
{
    /* TODO(Ballistic): MSR — same decode as get_sys_reg. */
    (void)s;
    (void)r;
    (void)v;
}

/*  Execution control  */

static void b_halt(CPU_State *s) { ((BalState *)s)->halted = true; }
static bool b_is_halted(CPU_State *s) { return ((BalState *)s)->halted; }

/*  Code-cache management  */

static void b_invalidate_cache(CPU_State *state, uint64_t addr, uint64_t size)
{
    /* TODO(Ballistic): drop blocks intersecting [addr, addr+size). With
     * dense-array + index-linked-list block storage (§7) this is an
     * O(touched) walk, not the O(all) walk dynarmic does today. */
    (void)state;
    (void)addr;
    (void)size;
}
static void b_clear_cache(CPU_State *state)
{
    /* TODO(Ballistic): drop all blocks, free executable memory. */
    (void)state;
}

/*  Callback registration  */

static void b_set_svc(CPU_State *s, CPU_SVC_Handler h) { ((BalState *)s)->on_svc = h; }
static void b_set_undef(CPU_State *s, CPU_Undefined_Handler h) { ((BalState *)s)->on_undefined = h; }
static void b_set_breakpoint(CPU_State *s, CPU_Breakpoint_Handler h) { ((BalState *)s)->on_breakpoint = h; }

/*  The vtable  */

const CPU_Backend CPU_BACKEND_BALLISTIC = {
    .create = b_create,
    .destroy = b_destroy,
    .run = b_run,
    .step = b_step,
    .run_until = b_run_until,
    .get_reg = b_get_reg,
    .set_reg = b_set_reg,
    .get_pc = b_get_pc,
    .set_pc = b_set_pc,
    .get_sp = b_get_sp,
    .set_sp = b_set_sp,
    .get_pstate = b_get_pstate,
    .set_pstate = b_set_pstate,
    .get_sys_reg = b_get_sys_reg,
    .set_sys_reg = b_set_sys_reg,
    .halt = b_halt,
    .is_halted = b_is_halted,
    .invalidate_cache = b_invalidate_cache,
    .clear_cache = b_clear_cache,
    .set_svc_handler = b_set_svc,
    .set_undefined_handler = b_set_undef,
    .set_breakpoint_handler = b_set_breakpoint,
    .name = "ballistic",
    .version = "0.0.0-stub",
    .supports_jit = false,  /* flip to true when run() works  */
    .supports_wasm = false, /* flip to true when WASM backend lands */
};
