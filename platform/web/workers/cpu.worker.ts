/**
 * CPU emulation worker. In Phase 0 the WASM core is not yet loaded - this
 * worker verifies that:
 *   - it receives the init message
 *   - the SharedArrayBuffers round-trip (we can write + read guestRAM[0])
 *   - Atomics + the frame-sync SAB are usable
 * and reports "ready" with a fake backend name so the main thread's boot
 * sequence can be validated end to end.
 *
 * Phase 1 will swap the fake loop for WebAssembly.instantiate(core.wasm)
 * + emulator_create_ffi + emulator_run_ffi.
 */

import type { CPUToMainMessage, MainToCPUMessage } from "@bindings/protocol";

const self: DedicatedWorkerGlobalScope =
  globalThis as unknown as DedicatedWorkerGlobalScope;

function log(level: "debug" | "info" | "warn" | "error", message: string): void {
  const msg: CPUToMainMessage = { type: "log", level, message };
  self.postMessage(msg);
}

function sanityCheckSharedMemory(guestRAM: SharedArrayBuffer, frameSync: SharedArrayBuffer): boolean {
  const ram = new Uint8Array(guestRAM, 0, 16);
  ram[0] = 0xab;
  ram[15] = 0xcd;
  if (ram[0] !== 0xab || ram[15] !== 0xcd) return false;
  ram[0] = 0;
  ram[15] = 0;

  const sync = new Int32Array(frameSync);
  Atomics.store(sync, 0, 0);
  if (Atomics.load(sync, 0) !== 0) return false;
  return true;
}

self.addEventListener("message", (event: MessageEvent<MainToCPUMessage>) => {
  const msg = event.data;

  if (msg.type === "init") {
    log("info", "cpu.worker initialising");

    const ok = sanityCheckSharedMemory(msg.shared.guestRAM, msg.shared.frameSync);
    if (!ok) {
      const errMsg: CPUToMainMessage = {
        type: "error",
        message: "shared memory round-trip failed",
      };
      self.postMessage(errMsg);
      return;
    }

    log("info", `guestRAM=${msg.shared.guestRAM.byteLength} bytes, shared memory ok`);

    const ready: CPUToMainMessage = {
      type: "ready",
      backendName:    "noop",
      backendVersion: "1.0.0",
    };
    self.postMessage(ready);
    return;
  }

  if (msg.type === "halt") {
    const halted: CPUToMainMessage = { type: "halted" };
    self.postMessage(halted);
    return;
  }
});
