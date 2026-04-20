#include "emulator.h"

#include "common/assert.h"
#include "common/log.h"

#include <stdlib.h>
#include <string.h>

Error emulator_create(Emulator* out, const Emulator_Config* config) {
  if (!out || !config) {
    return ERR(RESULT_INVALID_ARGUMENT, "emulator_create: out/config is NULL");
  }
  if (config->guest_ram_size == 0) {
    return ERR(RESULT_INVALID_ARGUMENT, "emulator_create: guest_ram_size is zero");
  }

  memset(out, 0, sizeof(*out));

  /* 1. Guest RAM. */
  if (config->guest_ram != NULL) {
    out->guest_ram      = config->guest_ram;
    out->owns_guest_ram = false;
  } else {
    out->guest_ram = calloc(1, (size_t)config->guest_ram_size);
    if (!out->guest_ram) {
      return ERR(RESULT_OUT_OF_MEMORY, "emulator_create: failed to allocate guest RAM");
    }
    out->owns_guest_ram = true;
  }
  out->guest_ram_size = config->guest_ram_size;

  /* 2. CPU backend selected at configure time. */
  out->cpu_backend = cpu_get_active_backend();
  SWITCH_ASSERT_ALWAYS(out->cpu_backend != NULL, "no CPU backend registered");

  out->cpu_state = out->cpu_backend->create(out->guest_ram,
                                            out->guest_ram_size,
                                            &out->hle);
  if (!out->cpu_state) {
    if (out->owns_guest_ram) free(out->guest_ram);
    memset(out, 0, sizeof(*out));
    return ERR(RESULT_OUT_OF_MEMORY, "emulator_create: CPU backend failed to init");
  }

  /* 3. HLE context + svc/undefined hooks. */
  hle_context_init(&out->hle, out->cpu_backend);
  out->cpu_backend->set_handler_userdata(out->cpu_state, &out->hle);
  out->cpu_backend->set_svc_handler(out->cpu_state, hle_on_svc);
  out->cpu_backend->set_undefined_handler(out->cpu_state, hle_on_undefined);

  log_info("[emulator] created (backend=%s %s, guest RAM=%llu MiB%s)",
           out->cpu_backend->name,
           out->cpu_backend->version,
           (unsigned long long)(out->guest_ram_size / (1024u * 1024u)),
           out->owns_guest_ram ? ", owned" : ", caller-owned");

  return OK;
}

void emulator_destroy(Emulator* emulator) {
  if (!emulator) return;
  if (emulator->cpu_backend && emulator->cpu_state) {
    emulator->cpu_backend->destroy(emulator->cpu_state);
  }
  if (emulator->owns_guest_ram && emulator->guest_ram) {
    free(emulator->guest_ram);
  }
  memset(emulator, 0, sizeof(*emulator));
}

void emulator_run(Emulator* emulator, uint64_t entry_point) {
  SWITCH_ASSERT_ALWAYS(emulator != NULL, "emulator_run: emulator is NULL");
  emulator->cpu_backend->run(emulator->cpu_state, entry_point);
}

void emulator_step(Emulator* emulator) {
  SWITCH_ASSERT_ALWAYS(emulator != NULL, "emulator_step: emulator is NULL");
  emulator->cpu_backend->step(emulator->cpu_state);
}

void emulator_halt(Emulator* emulator) {
  if (!emulator || !emulator->cpu_backend || !emulator->cpu_state) return;
  emulator->cpu_backend->halt(emulator->cpu_state);
}

const char* emulator_backend_name(const Emulator* emulator) {
  if (!emulator || !emulator->cpu_backend) return "<none>";
  return emulator->cpu_backend->name;
}
