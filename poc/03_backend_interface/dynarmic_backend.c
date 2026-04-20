/*
 * dynarmic_backend.c — wires the dynarmic recompiler into CPU_Backend.
 *
 * dynarmic is C++. The actual class instantiation (Dynarmic::A64::Jit,
 * Dynarmic::A64::UserConfig, the UserCallbacks subclass) lives in a
 * companion dynarmic_shim.cpp that exposes a flat C ABI through the
 * `dyn_*` symbols declared `extern` below. This file is pure C and does
 * exactly one thing: forward each vtable slot to the corresponding shim
 * function. It is the "thin C wrapper" the project promises in §3 of
 * DESIGN.MD.
 *
 * What this file demonstrates for the Ballistic side: the surface to
 * implement is small. Everything fits in one screen. Replace `dyn_*`
 * with `bal_*` and you are done — that is exactly what
 * ballistic_stub.c does in this same directory.
 */

#include "cpu.h"

#include <stdlib.h>

/*  C ABI exposed by dynarmic_shim.cpp
 *
 * Each dyn_* call corresponds 1:1 to a Dynarmic::A64::Jit method. The
 * shim owns the C++ object; this file only sees opaque pointers.
 */
extern void *dyn_create(void *guest_ram, uint64_t ram_size, void *userdata);
extern void dyn_destroy(void *jit);

extern void dyn_run(void *jit, uint64_t entry_point);
extern void dyn_step(void *jit);
extern void dyn_run_until(void *jit, uint64_t address);

extern uint64_t dyn_get_reg(void *jit, uint8_t i);
extern void dyn_set_reg(void *jit, uint8_t i, uint64_t v);
extern uint64_t dyn_get_pc(void *jit);
extern void dyn_set_pc(void *jit, uint64_t v);
extern uint64_t dyn_get_sp(void *jit);
extern void dyn_set_sp(void *jit, uint64_t v);
extern uint32_t dyn_get_pstate(void *jit);
extern void dyn_set_pstate(void *jit, uint32_t v);
extern uint64_t dyn_get_sys_reg(void *jit, uint32_t r);
extern void dyn_set_sys_reg(void *jit, uint32_t r, uint64_t v);

extern void dyn_halt(void *jit);
extern bool dyn_is_halted(void *jit);

extern void dyn_invalidate_cache(void *jit, uint64_t addr, uint64_t size);
extern void dyn_clear_cache(void *jit);

extern void dyn_set_svc_handler(void *jit, CPU_SVC_Handler h);
extern void dyn_set_undefined_handler(void *jit, CPU_Undefined_Handler h);
extern void dyn_set_breakpoint_handler(void *jit, CPU_Breakpoint_Handler h);

/*  Thunks: CPU_State* is just a typed alias for the shim handle.  */

static CPU_State *d_create(void *ram, uint64_t size, void *ud)
{
    return (CPU_State *)dyn_create(ram, size, ud);
}
static void d_destroy(CPU_State *s) { dyn_destroy(s); }
static void d_run(CPU_State *s, uint64_t e) { dyn_run(s, e); }
static void d_step(CPU_State *s) { dyn_step(s); }
static void d_run_until(CPU_State *s, uint64_t a) { dyn_run_until(s, a); }

static uint64_t d_get_reg(CPU_State *s, uint8_t i) { return dyn_get_reg(s, i); }
static void d_set_reg(CPU_State *s, uint8_t i, uint64_t v) { dyn_set_reg(s, i, v); }
static uint64_t d_get_pc(CPU_State *s) { return dyn_get_pc(s); }
static void d_set_pc(CPU_State *s, uint64_t v) { dyn_set_pc(s, v); }
static uint64_t d_get_sp(CPU_State *s) { return dyn_get_sp(s); }
static void d_set_sp(CPU_State *s, uint64_t v) { dyn_set_sp(s, v); }
static uint32_t d_get_pstate(CPU_State *s) { return dyn_get_pstate(s); }
static void d_set_pstate(CPU_State *s, uint32_t v) { dyn_set_pstate(s, v); }
static uint64_t d_get_sys_reg(CPU_State *s, uint32_t r) { return dyn_get_sys_reg(s, r); }
static void d_set_sys_reg(CPU_State *s, uint32_t r, uint64_t v) { dyn_set_sys_reg(s, r, v); }

static void d_halt(CPU_State *s) { dyn_halt(s); }
static bool d_is_halted(CPU_State *s) { return dyn_is_halted(s); }
static void d_invalidate_cache(CPU_State *s, uint64_t a, uint64_t sz) { dyn_invalidate_cache(s, a, sz); }
static void d_clear_cache(CPU_State *s) { dyn_clear_cache(s); }

static void d_set_svc(CPU_State *s, CPU_SVC_Handler h) { dyn_set_svc_handler(s, h); }
static void d_set_undef(CPU_State *s, CPU_Undefined_Handler h) { dyn_set_undefined_handler(s, h); }
static void d_set_breakpoint(CPU_State *s, CPU_Breakpoint_Handler h) { dyn_set_breakpoint_handler(s, h); }

/* The vtable. This is the only symbol exported to the rest of the core. */
const CPU_Backend CPU_BACKEND_DYNARMIC = {
    .create = d_create,
    .destroy = d_destroy,
    .run = d_run,
    .step = d_step,
    .run_until = d_run_until,
    .get_reg = d_get_reg,
    .set_reg = d_set_reg,
    .get_pc = d_get_pc,
    .set_pc = d_set_pc,
    .get_sp = d_get_sp,
    .set_sp = d_set_sp,
    .get_pstate = d_get_pstate,
    .set_pstate = d_set_pstate,
    .get_sys_reg = d_get_sys_reg,
    .set_sys_reg = d_set_sys_reg,
    .halt = d_halt,
    .is_halted = d_is_halted,
    .invalidate_cache = d_invalidate_cache,
    .clear_cache = d_clear_cache,
    .set_svc_handler = d_set_svc,
    .set_undefined_handler = d_set_undef,
    .set_breakpoint_handler = d_set_breakpoint,
    .name = "dynarmic",
    .version = "6.7.0",
    .supports_jit = true,
    .supports_wasm = false,
};
