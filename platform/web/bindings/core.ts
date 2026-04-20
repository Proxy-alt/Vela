/**
 * Typed surface of the WASM core's exported functions. The actual module
 * is produced by `emcmake cmake ... && cmake --build build` which emits
 * `switch_core.js` + `switch_core.wasm`. Phase 0 does not load it yet -
 * this file declares the shape so the CPU worker can wire against it when
 * Phase 1 arrives.
 */

export interface SwitchCoreExports {
  readonly _emulator_create_ffi:  (guestRamSize: bigint) => number;
  readonly _emulator_destroy_ffi: () => void;
  readonly _emulator_run_ffi:     (entryPoint: bigint) => void;
  readonly _emulator_step_ffi:    () => void;

  readonly _cpu_get_reg_ffi: (index: number) => bigint;
  readonly _cpu_set_reg_ffi: (index: number, value: bigint) => void;
  readonly _cpu_get_pc_ffi:  () => bigint;
}

/* ARM64 register indices. Mirrors core/cpu/cpu.h. */
export const CPU_REG_X0  = 0;
export const CPU_REG_X8  = 8;
export const CPU_REG_X29 = 29;
export const CPU_REG_X30 = 30;
export const CPU_REG_XZR = 31;
