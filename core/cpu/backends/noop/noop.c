/**
 * Implementation of the no-op CPU backend. See docs/DESIGN.md section 6.
 * Executes zero ARM instructions but faithfully tracks register state so
 * HLE services, loader code, and worker plumbing can all be validated
 * before the interpreter/recompiler lands.
 */
#include "cpu/backends/noop/noop.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>

typedef struct NoopState
{
  uint64_t general_regs[31]; /* X0..X30 */
  uint64_t pc;
  uint64_t sp;
  uint32_t pstate;

  bool halted;

  void *guest_ram;
  uint64_t ram_size;

  void *handler_userdata;
  CPU_SVC_Handler svc_handler;
  CPU_Undefined_Handler undefined_handler;
  CPU_Breakpoint_Handler breakpoint_handler;
} NoopState;

static CPU_State *noop_create(void *guest_ram, uint64_t ram_size, void *userdata)
{
  NoopState *state = (NoopState *)calloc(1, sizeof(NoopState));
  if (!state)
    return NULL;
  state->guest_ram = guest_ram;
  state->ram_size = ram_size;
  state->handler_userdata = userdata;
  state->halted = false;
  return (CPU_State *)state;
}

static void noop_destroy(CPU_State *state)
{
  free(state);
}

static void noop_run(CPU_State *state, uint64_t entry_point)
{
  NoopState *s = (NoopState *)state;
  s->pc = entry_point;
  log_debug("[cpu/noop] run() called at guest PC 0x%016llx, no instructions executed",
            (unsigned long long)entry_point);
  /* HLE services still fire because the emulator drives them directly through
   * the SVC handler rather than through guest instruction execution. */
}

static void noop_step(CPU_State *state) { (void)state; }
static void noop_run_until(CPU_State *state, uint64_t pc)
{
  (void)state;
  (void)pc;
}

static uint64_t noop_get_reg(CPU_State *state, uint8_t index)
{
  if (index >= 31)
    return 0; /* XZR */
  return ((NoopState *)state)->general_regs[index];
}

static void noop_set_reg(CPU_State *state, uint8_t index, uint64_t value)
{
  if (index >= 31)
    return; /* XZR writes are discarded */
  ((NoopState *)state)->general_regs[index] = value;
}

static uint64_t noop_get_pc(CPU_State *state) { return ((NoopState *)state)->pc; }
static void noop_set_pc(CPU_State *state, uint64_t v) { ((NoopState *)state)->pc = v; }
static uint64_t noop_get_sp(CPU_State *state) { return ((NoopState *)state)->sp; }
static void noop_set_sp(CPU_State *state, uint64_t v) { ((NoopState *)state)->sp = v; }
static uint32_t noop_get_pstate(CPU_State *state) { return ((NoopState *)state)->pstate; }
static void noop_set_pstate(CPU_State *state, uint32_t v) { ((NoopState *)state)->pstate = v; }

static uint64_t noop_get_sys_reg(CPU_State *state, uint32_t reg)
{
  (void)state;
  (void)reg;
  return 0;
}
static void noop_set_sys_reg(CPU_State *state, uint32_t reg, uint64_t value)
{
  (void)state;
  (void)reg;
  (void)value;
}

static void noop_halt(CPU_State *state) { ((NoopState *)state)->halted = true; }
static bool noop_is_halted(CPU_State *state) { return ((NoopState *)state)->halted; }

static void noop_invalidate_cache(CPU_State *state, uint64_t addr, uint64_t size)
{
  (void)state;
  (void)addr;
  (void)size;
}
static void noop_clear_cache(CPU_State *state) { (void)state; }

static void noop_set_svc_handler(CPU_State *state, CPU_SVC_Handler h)
{
  ((NoopState *)state)->svc_handler = h;
}
static void noop_set_undefined_handler(CPU_State *state, CPU_Undefined_Handler h)
{
  ((NoopState *)state)->undefined_handler = h;
}
static void noop_set_breakpoint_handler(CPU_State *state, CPU_Breakpoint_Handler h)
{
  ((NoopState *)state)->breakpoint_handler = h;
}
static void noop_set_handler_userdata(CPU_State *state, void *userdata)
{
  ((NoopState *)state)->handler_userdata = userdata;
}

const CPU_Backend CPU_BACKEND_NOOP = {
    .create = noop_create,
    .destroy = noop_destroy,
    .run = noop_run,
    .step = noop_step,
    .run_until = noop_run_until,
    .get_reg = noop_get_reg,
    .set_reg = noop_set_reg,
    .get_pc = noop_get_pc,
    .set_pc = noop_set_pc,
    .get_sp = noop_get_sp,
    .set_sp = noop_set_sp,
    .get_pstate = noop_get_pstate,
    .set_pstate = noop_set_pstate,
    .get_sys_reg = noop_get_sys_reg,
    .set_sys_reg = noop_set_sys_reg,
    .halt = noop_halt,
    .is_halted = noop_is_halted,
    .invalidate_cache = noop_invalidate_cache,
    .clear_cache = noop_clear_cache,
    .set_svc_handler = noop_set_svc_handler,
    .set_undefined_handler = noop_set_undefined_handler,
    .set_breakpoint_handler = noop_set_breakpoint_handler,
    .set_handler_userdata = noop_set_handler_userdata,
    .name = "noop",
    .version = "1.0.0",
    .supports_jit = false,
    .supports_wasm = false,
};
