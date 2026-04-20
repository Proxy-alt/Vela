#include "cpu/cpu.h"
#include "common/log.h"

const CPU_Backend* cpu_get_active_backend(void) {
#if defined(SWITCH_CPU_BACKEND_NOOP)
  return &CPU_BACKEND_NOOP;
#else
# error "No CPU_BACKEND_* define set. Configure the build with -DCPU_BACKEND=noop (or a real backend)."
#endif
}
