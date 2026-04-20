/**
 * Top-level emulator wiring. The core entry point for every platform.
 * Phase 0 scope: create/destroy, wire the no-op CPU backend into the stub
 * HLE dispatcher, expose run/step/reset.
 */
#ifndef SWITCH_EMULATOR_H
#define SWITCH_EMULATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "common/result.h"
#include "cpu/cpu.h"
#include "hle/hle.h"

typedef struct Emulator
{
  const CPU_Backend *cpu_backend;
  CPU_State *cpu_state;
  HLE_Context hle;

  void *guest_ram;
  uint64_t guest_ram_size;
  bool owns_guest_ram;
} Emulator;

typedef struct Emulator_Config
{
  /* Guest RAM. If `guest_ram` is NULL the emulator allocates `guest_ram_size`
   * bytes itself; otherwise the caller (e.g. the web worker, which supplies
   * a SharedArrayBuffer) owns the memory. */
  void *guest_ram;
  uint64_t guest_ram_size;
} Emulator_Config;

/* Default guest RAM for Switch 1: 4 GB. */
#define EMULATOR_DEFAULT_GUEST_RAM_BYTES ((uint64_t)4 * 1024 * 1024 * 1024)

Error emulator_create(Emulator *out, const Emulator_Config *config);
void emulator_destroy(Emulator *emulator);

/* Run the guest from `entry_point` until the CPU halts. Phase 0: returns
 * immediately because the no-op backend executes zero instructions. */
void emulator_run(Emulator *emulator, uint64_t entry_point);

/* Single-step. */
void emulator_step(Emulator *emulator);

/* Stop the guest. Safe to call from any thread. */
void emulator_halt(Emulator *emulator);

/* Diagnostics. */
const char *emulator_backend_name(const Emulator *emulator);

#endif /* SWITCH_EMULATOR_H */
