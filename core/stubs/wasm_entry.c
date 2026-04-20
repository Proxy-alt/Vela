/**
 * Thin Emscripten entry point. Exposes the emulator public API as exported
 * C functions so the CPU worker can call into the core over ccall/cwrap.
 * See docs/DESIGN.md section 20 (WASM build flags, EXPORTED_FUNCTIONS).
 */
#include "common/log.h"
#include "emulator.h"

#include <stddef.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

static Emulator g_emulator;
static int g_initialised = 0;

EXPORT int emulator_create_ffi(uint64_t guest_ram_size_bytes)
{
  if (g_initialised)
    return 1;
  const Emulator_Config cfg = {
      .guest_ram = NULL,
      .guest_ram_size = guest_ram_size_bytes,
  };
  const Error err = emulator_create(&g_emulator, &cfg);
  if (err.code != RESULT_OK)
  {
    log_error("[wasm_entry] emulator_create failed: %s", err.message);
    return 0;
  }
  g_initialised = 1;
  return 1;
}

EXPORT void emulator_destroy_ffi(void)
{
  if (!g_initialised)
    return;
  emulator_destroy(&g_emulator);
  g_initialised = 0;
}

EXPORT void emulator_run_ffi(uint64_t entry_point)
{
  if (!g_initialised)
    return;
  emulator_run(&g_emulator, entry_point);
}

EXPORT void emulator_step_ffi(void)
{
  if (!g_initialised)
    return;
  emulator_step(&g_emulator);
}

EXPORT uint64_t cpu_get_reg_ffi(uint32_t index)
{
  if (!g_initialised)
    return 0;
  return g_emulator.cpu_backend->get_reg(g_emulator.cpu_state, (uint8_t)index);
}

EXPORT void cpu_set_reg_ffi(uint32_t index, uint64_t value)
{
  if (!g_initialised)
    return;
  g_emulator.cpu_backend->set_reg(g_emulator.cpu_state, (uint8_t)index, value);
}

EXPORT uint64_t cpu_get_pc_ffi(void)
{
  if (!g_initialised)
    return 0;
  return g_emulator.cpu_backend->get_pc(g_emulator.cpu_state);
}

#ifndef __EMSCRIPTEN__
int main(void)
{
  log_info("[wasm_entry] native build, running smoke test");
  const Emulator_Config cfg = {
      .guest_ram = NULL,
      .guest_ram_size = 4u * 1024u * 1024u,
  };
  Emulator emu;
  const Error err = emulator_create(&emu, &cfg);
  if (err.code != RESULT_OK)
  {
    log_error("emulator_create failed: %s", err.message);
    return 1;
  }
  emulator_run(&emu, 0x1000);
  emulator_destroy(&emu);
  return 0;
}
#endif
