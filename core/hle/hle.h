/**
 * Stub HLE dispatcher for Phase 0. Real syscall implementations arrive in
 * later phases (see docs/DESIGN.md section 8 for the priority list).
 *
 * For now the dispatcher:
 *   - owns an HLE_Context that holds the active CPU backend vtable
 *   - intercepts every SVC via the CPU backend's svc handler
 *   - logs the syscall id and writes RESULT_NOT_IMPLEMENTED back into X0
 */
#ifndef SWITCH_HLE_HLE_H
#define SWITCH_HLE_HLE_H

#include "cpu/cpu.h"

/* Switch OS result codes (subset relevant to Phase 0). */
#define HLE_RESULT_SUCCESS 0x00000000u
#define HLE_RESULT_NOT_IMPLEMENTED 0x0000F601u
#define HLE_RESULT_INVALID_HANDLE 0x0000E401u

typedef struct HLE_Context
{
  const CPU_Backend *cpu_backend;
  uint64_t svc_call_count;
} HLE_Context;

void hle_context_init(HLE_Context *context, const CPU_Backend *backend);

/* CPU_SVC_Handler-compatible entry point. */
void hle_on_svc(CPU_State *cpu_state, uint32_t swi, void *userdata);

/* CPU_Undefined_Handler-compatible entry point. */
void hle_on_undefined(CPU_State *cpu_state, uint32_t instruction, void *userdata);

#endif /* SWITCH_HLE_HLE_H */
