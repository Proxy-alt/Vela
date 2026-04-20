/**
 * Abstract CPU backend interface. This is the most important abstraction
 * in the project: every backend (noop, interpreter, dynarmic, ballistic)
 * implements it exactly. Nothing outside core/cpu/ should care which
 * backend is active.
 *
 * See docs/DESIGN.md section 5.
 */
#ifndef SWITCH_CPU_CPU_H
#define SWITCH_CPU_CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CPU_State CPU_State;
typedef struct CPU_Backend CPU_Backend;

/* ------------------------------------------------------------------ */
/* Callbacks set by the emulator core, invoked by the backend.         */
/* ------------------------------------------------------------------ */

typedef void (*CPU_SVC_Handler)(CPU_State *state, uint32_t swi, void *userdata);
typedef void (*CPU_Undefined_Handler)(CPU_State *state, uint32_t instruction, void *userdata);
typedef void (*CPU_Breakpoint_Handler)(CPU_State *state, uint64_t address, void *userdata);

/* ------------------------------------------------------------------ */
/* Register index conventions (ARM64).                                 */
/* ------------------------------------------------------------------ */

#define CPU_REG_X0 0u
#define CPU_REG_X8 8u   /* syscall number */
#define CPU_REG_X29 29u /* frame pointer  */
#define CPU_REG_X30 30u /* link register  */
#define CPU_REG_XZR 31u /* zero register  */

/* ------------------------------------------------------------------ */
/* Backend interface vtable.                                           */
/* ------------------------------------------------------------------ */

struct CPU_Backend
{
  /* Lifecycle */
  CPU_State *(*create)(void *guest_ram, uint64_t ram_size, void *userdata);
  void (*destroy)(CPU_State *state);

  /* Execution */
  void (*run)(CPU_State *state, uint64_t entry_point);
  void (*step)(CPU_State *state);
  void (*run_until)(CPU_State *state, uint64_t address);

  /* General purpose registers */
  uint64_t (*get_reg)(CPU_State *state, uint8_t reg_index);
  void (*set_reg)(CPU_State *state, uint8_t reg_index, uint64_t value);

  /* Special registers */
  uint64_t (*get_pc)(CPU_State *state);
  void (*set_pc)(CPU_State *state, uint64_t value);
  uint64_t (*get_sp)(CPU_State *state);
  void (*set_sp)(CPU_State *state, uint64_t value);
  uint32_t (*get_pstate)(CPU_State *state);
  void (*set_pstate)(CPU_State *state, uint32_t value);

  /* System registers (MRS/MSR) */
  uint64_t (*get_sys_reg)(CPU_State *state, uint32_t encoded_reg);
  void (*set_sys_reg)(CPU_State *state, uint32_t encoded_reg, uint64_t value);

  /* Execution control */
  void (*halt)(CPU_State *state);
  bool (*is_halted)(CPU_State *state);

  /* Code cache management */
  void (*invalidate_cache)(CPU_State *state, uint64_t guest_address, uint64_t size_bytes);
  void (*clear_cache)(CPU_State *state);

  /* Callback registration */
  void (*set_svc_handler)(CPU_State *state, CPU_SVC_Handler handler);
  void (*set_undefined_handler)(CPU_State *state, CPU_Undefined_Handler handler);
  void (*set_breakpoint_handler)(CPU_State *state, CPU_Breakpoint_Handler handler);
  void (*set_handler_userdata)(CPU_State *state, void *userdata);

  /* Metadata */
  const char *name;
  const char *version;
  bool supports_jit;
  bool supports_wasm;
};

/* ------------------------------------------------------------------ */
/* Backend registry.                                                   */
/* ------------------------------------------------------------------ */

extern const CPU_Backend CPU_BACKEND_NOOP;

/* Returns the backend selected at configure time (see CPU_BACKEND CMake option). */
const CPU_Backend *cpu_get_active_backend(void);

#endif /* SWITCH_CPU_CPU_H */
