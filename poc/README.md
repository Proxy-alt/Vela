# Ballistic integration POC

This is a pitch, not a fork request. Three small artifacts demonstrating
that a Switch emulator core has already been designed around a
recompiler-shaped hole, that the hole fits Ballistic exactly, and that
the WASM backend you'll need to ship to be the primary recompiler for
that emulator is a finite, scoped piece of work - not a research
project.

Read in order:

```
01_dynarmic_bottlenecks/   what is broken about dynarmic, in numbers
02_wasm_pipeline/          proof the WASM JIT path actually works
03_backend_interface/      the C surface you implement against
```

---

## 1. What exists now

A Switch emulator core (this repository) built on the assumption that
the recompiler is a swappable backend behind a vtable. Today the build
works with a no-op backend (every subsystem buildable, no game
execution) and an interpreter backend (slow, correct, unblocks HLE
work). dynarmic is wired in as the interim desktop recompiler.

The vtable is [poc/03_backend_interface/cpu.h](03_backend_interface/cpu.h).
That is the entire surface the rest of the core sees. HLE, GPU, audio,
loader - none of them know which backend is active.

---

## 2. What is wrong with dynarmic, with numbers

[poc/01_dynarmic_bottlenecks/bench.cpp](01_dynarmic_bottlenecks/bench.cpp)
is a synthetic benchmark - no ROM, no HLE, no game, just a 16-ADD
hot block fed to `Dynarmic::A64::Jit::Run()` in a tight loop with
profiling around it. Captured output is in
[sample_output.txt](01_dynarmic_bottlenecks/sample_output.txt);
build instructions in [CMakeLists.txt](01_dynarmic_bottlenecks/CMakeLists.txt).
Three numbers come out, all matching the ones you've already
characterised yourselves:

| metric                            | dynarmic        | source of cost                                      |
|-----------------------------------|-----------------|-----------------------------------------------------|
| `sizeof(Jit)` cache lines         | **4** (248 B)   | JIT-state struct sprawls past 64 B, ≥1 extra L1 miss per entry |
| prologue / epilogue cycles / Run  | **~54**         | dispatcher save/restore + register-file reload      |
| invalidate+Run vs cached Run      | **~118x**       | block cache is an intrusive linked list; every invalidation walks pointers and forces re-trace on next entry |

These are the costs you cannot optimise away inside dynarmic without
breaking it. They are properties of the data layout and the IR.

---

## 3. What Ballistic fixes, mapped to those numbers

Per the project design doc (§7), Ballistic targets exactly these:

| dynarmic cost            | Ballistic design choice that removes it                          |
|--------------------------|------------------------------------------------------------------|
| 4 cache lines / Run      | JIT state packed within a single 64 B line                       |
| 54 cycle entry tax       | C, no virtual dispatch, base+index addressing - no 8 B pointer loads on the dispatcher's hot path |
| 118x eviction slowdown   | dense array + index linked lists for blocks; invalidation is O(touched), not O(all) |

The peephole pass and correct XMM/SIMD spilling are bonus wins not
captured by this benchmark.

---

## 4. What Ballistic needs to implement

Two things. The first is mechanical. The second is design and is the
reason this POC exists.

### 4a. The vtable

[poc/03_backend_interface/cpu.h](03_backend_interface/cpu.h) is the C
surface. [dynarmic_backend.c](03_backend_interface/dynarmic_backend.c)
shows it filled in for dynarmic (one screen of thunks).
[ballistic_stub.c](03_backend_interface/ballistic_stub.c) is the same
file with every function stubbed and a `TODO(Ballistic)` describing
what to put there. It compiles today. Replacing
`CPU_BACKEND_DYNARMIC` with `CPU_BACKEND_BALLISTIC` in the core's
backend-selection table is a one-line change. Nothing else moves.

### 4b. Three IR requirements (raise on Pound Discord before the IR solidifies)

These exist because of a single downstream constraint: a WASM backend.
[poc/02_wasm_pipeline/index.html](02_wasm_pipeline/index.html) is a
self-contained page - no build, no deps - that hand-emits a WASM
module representing one ARM `ADD`, hands it to
`WebAssembly.compile()`, instantiates it, calls it. Open in Chrome,
look at the console. That same byte stream is what the Ballistic WASM
backend would emit per translated ARM block. The pipeline works. The
question is whether Ballistic's IR will let you write the emitter at
all.

**1. CFG must be preserved through to the backend.** WASM has no
`goto`. Structured control flow is recovered from a CFG by the
Relooper algorithm. If the IR collapses branch edges into
`{op: JMP, target}` instructions before the backend sees them, the
CFG is gone and Relooper cannot run. Required shape: blocks own
their successors and branch condition explicitly.

**2. SSA value numbering, not a fixed register file.** WASM is a
stack machine; SSA values map directly to WASM locals with no
register allocator. A fixed-register IR forces the WASM backend to
ship its own allocator that exists only to undo the previous one.

**3. Backend as a vtable, not compile-time selection.** WASM has to
sit alongside x64 and arm64 as an equal target. If the backend is
chosen by `#ifdef`, every new target requires editing the IR core.
A `Ballistic_Backend` struct of function pointers
(`alloc_executable`, `emit_instruction`, `emit_branch`, `finalise`)
makes adding WASM a new file, not a refactor.

The Pound Discord questions, exactly:

> 1. "Is the CFG being preserved as explicit block edges through to the
>    backend, or flattened to jumps before emission?"
> 2. "Are instruction results represented as SSA values or writes to a
>    fixed register file?"
> 3. "Is the backend interface a vtable of function pointers or
>    compiled-in per build target?"

If the answers are *yes / SSA / vtable*, the WASM backend is a
straightforward project. If any answer is *no*, fixing it later means
restructuring the compilation pipeline - much cheaper to settle now.
