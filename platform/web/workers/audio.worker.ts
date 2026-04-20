/**
 * Audio worker. Phase 0: receive the ring buffer handle and report "ready".
 * AudioWorkletProcessor wiring, sample-rate negotiation, and DSP arrive in
 * Phase 4+ (docs/DESIGN.md section 10).
 */

import type { AudioToMainMessage, MainToAudioMessage } from "@bindings/protocol";

const self: DedicatedWorkerGlobalScope =
  globalThis as unknown as DedicatedWorkerGlobalScope;

self.addEventListener("message", (event: MessageEvent<MainToAudioMessage>) => {
  if (event.data.type !== "init") return;

  const log: AudioToMainMessage = {
    type: "log",
    level: "info",
    message: `audio worker initialised, ring=${event.data.audioRing.byteLength} bytes`,
  };
  self.postMessage(log);

  const ready: AudioToMainMessage = { type: "ready" };
  self.postMessage(ready);
});
