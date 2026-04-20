/**
 * End-to-end smoke test for Phase 0. Exercises the full path:
 *   emulator_create -> set register -> run (no-op) -> fire an SVC manually ->
 *   confirm the HLE dispatcher ran and wrote NOT_IMPLEMENTED into X0.
 */
#include "common/log.h"
#include "cpu/cpu.h"
#include "emulator.h"
#include "hle/hle.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                   \
  do                                                                  \
  {                                                                   \
    if (!(cond))                                                      \
    {                                                                 \
      fprintf(stderr, "[smoke] FAIL %s:%d: %s\n", __FILE__, __LINE__, \
              #cond);                                                 \
      exit(1);                                                        \
    }                                                                 \
  } while (0)

int main(void)
{
  const Emulator_Config cfg = {
      .guest_ram = NULL,
      .guest_ram_size = 1024u * 1024u, /* 1 MiB is plenty for Phase 0. */
  };

  Emulator emu;
  Error err = emulator_create(&emu, &cfg);
  CHECK(err.code == RESULT_OK);
  CHECK(emu.cpu_backend != NULL);
  CHECK(emu.cpu_state != NULL);

  const CPU_Backend *cpu = emu.cpu_backend;

  /* Register read/write round-trip. */
  cpu->set_reg(emu.cpu_state, CPU_REG_X0, 0xdeadbeefcafebabeULL);
  CHECK(cpu->get_reg(emu.cpu_state, CPU_REG_X0) == 0xdeadbeefcafebabeULL);

  /* XZR must always read zero and swallow writes. */
  cpu->set_reg(emu.cpu_state, CPU_REG_XZR, 0xffffffffffffffffULL);
  CHECK(cpu->get_reg(emu.cpu_state, CPU_REG_XZR) == 0);

  /* PC round-trip. */
  cpu->set_pc(emu.cpu_state, 0x1000);
  CHECK(cpu->get_pc(emu.cpu_state) == 0x1000);

  /* emulator_run should not crash under the no-op backend. */
  emulator_run(&emu, 0x1000);

  /* Simulate the backend calling the SVC handler: X8 holds the syscall id. */
  cpu->set_reg(emu.cpu_state, CPU_REG_X8, 0x01 /* SetHeapSize */);
  cpu->set_reg(emu.cpu_state, CPU_REG_X0, 0);
  hle_on_svc(emu.cpu_state, 0, &emu.hle);
  CHECK(cpu->get_reg(emu.cpu_state, CPU_REG_X0) == HLE_RESULT_NOT_IMPLEMENTED);
  CHECK(emu.hle.svc_call_count == 1);

  emulator_destroy(&emu);
  printf("[smoke] OK\n");
  return 0;
}
