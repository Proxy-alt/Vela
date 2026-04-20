/**
 * Phase 0 boot sequence. The page loads as a black screen with a boot log
 * on the right; once the CPU, GPU, and audio workers each report "ready"
 * we dynamically import the Solid shell and fade the boot overlay out.
 * Keeping the Solid runtime out of the boot-critical path means the black
 * screen appears immediately and the UI chunk only loads when the core
 * has actually come up.
 */

import type {
  AudioToMainMessage,
  CPUToMainMessage,
  GPUToMainMessage,
  MainToAudioMessage,
  MainToCPUMessage,
  MainToGPUMessage,
  SharedMemory,
} from "@bindings/protocol";
import { detectCapabilities, type PlatformCapabilities } from "./capabilities";
import { appendLogLine, setStatus } from "./log";

/** Guest memory sizing. 4 GB guest RAM requires memory64 + SharedArrayBuffer;
 * for Phase 0 we only touch a small slice, but the allocation size matches
 * the Switch 1 RAM budget so we catch out-of-memory problems early. */
const GUEST_RAM_BYTES   = 4 * 1024 * 1024 * 1024;
const GUEST_RAM_BYTES_FALLBACK = 16 * 1024 * 1024; // for testing on devices that can't do 4 GB
const FRAME_SYNC_BYTES  = 4;
const AUDIO_RING_BYTES  = 1 * 1024 * 1024;
const TRACE_BUFFER_BYTES = 2 * 1024 * 1024;

const WORKER_READY_TIMEOUT_MS = 10_000;

function fatal(reason: string): void {
  setStatus(`fatal: ${reason}`);
  appendLogLine("error", reason);
}

function reportCapabilities(caps: PlatformCapabilities): void {
  const lines: string[] = [];
  (Object.keys(caps) as Array<keyof PlatformCapabilities>).forEach(key => {
    lines.push(`${key}=${caps[key] ? "yes" : "no"}`);
  });
  appendLogLine("debug", `capabilities: ${lines.join(", ")}`);
}

function allocateSharedMemory(): SharedMemory | null {
  try {
    return {
      guestRAM:  new SharedArrayBuffer(GUEST_RAM_BYTES),
      frameSync: new SharedArrayBuffer(FRAME_SYNC_BYTES),
      audioRing: new SharedArrayBuffer(AUDIO_RING_BYTES),
      traceBuf:  new SharedArrayBuffer(TRACE_BUFFER_BYTES),
    };
  } catch {
    appendLogLine("warn", "4 GiB allocation failed, falling back to 16 MiB");
    try {
      return {
        guestRAM:  new SharedArrayBuffer(GUEST_RAM_BYTES_FALLBACK),
        frameSync: new SharedArrayBuffer(FRAME_SYNC_BYTES),
        audioRing: new SharedArrayBuffer(AUDIO_RING_BYTES),
        traceBuf:  new SharedArrayBuffer(TRACE_BUFFER_BYTES),
      };
    } catch (e) {
      appendLogLine("error", `Fallback SharedArrayBuffer allocation failed: ${(e as Error).message}`);
      return null;
    }
  }
}

function attachWorkerLogRelay<TMessage extends { type: string }>(
  workerName: string,
  worker: Worker,
  readyHandler: (msg: TMessage) => void,
  failHandler: (reason: string) => void,
): void {
  worker.addEventListener("message", (event: MessageEvent<TMessage>) => {
    const msg = event.data;
    if (msg.type === "log") {
      const logMsg = msg as TMessage & { level: "debug" | "info" | "warn" | "error"; message: string };
      appendLogLine(logMsg.level, `${workerName}: ${logMsg.message}`);
      return;
    }
    if (msg.type === "error") {
      const errMsg = msg as TMessage & { message: string };
      appendLogLine("error", `${workerName} error: ${errMsg.message}`);
      failHandler(errMsg.message);
      return;
    }
    readyHandler(msg);
  });
  worker.addEventListener("error", (event: ErrorEvent) => {
    appendLogLine("error", `${workerName} uncaught: ${event.message}`);
    failHandler(event.message || "uncaught worker error");
  });
}

interface BootResult {
  readonly adapterLabel: string;
  readonly cpuBackend:   string;
  readonly ramMiB:       number;
}

async function boot(): Promise<BootResult | null> {
  appendLogLine("info", "Vela web - Phase 0 skeleton booting");

  /* 1. Capability detection. */
  const caps = detectCapabilities();
  reportCapabilities(caps);

  if (!caps.sharedArrayBuffer) {
    fatal("cross-origin isolation inactive, SharedArrayBuffer is unavailable. " +
          "Serve with COOP: same-origin and COEP: require-corp.");
    return null;
  }
  if (!caps.webGPU) {
    appendLogLine("warn", "WebGPU unavailable. GPU worker will not produce pixels, but the boot flow will still validate.");
  }

  /* 2. Shared memory. */
  const shared = allocateSharedMemory();
  if (!shared) {
    fatal("failed to allocate guest RAM (4 GiB SharedArrayBuffer). Close tabs or lower the size.");
    return null;
  }
  const ramMiB = Math.round(shared.guestRAM.byteLength / (1024 * 1024));
  appendLogLine("info", `allocated guest RAM: ${ramMiB} MiB`);

  /* 3. Phase 0 GPU worker sanity: the framebuffer is not yet shown in the
   *    DOM. We hand the worker a detached OffscreenCanvas so the WebGPU
   *    init still runs against a real device; Phase 2+ will move the
   *    canvas into the Solid shell and route pixels out. */
  const scratchCanvas = document.createElement("canvas");
  scratchCanvas.width  = 1280;
  scratchCanvas.height = 720;
  const offscreen = scratchCanvas.transferControlToOffscreen();

  /* 4. Workers. Vite's `new URL(..., import.meta.url)` + `type: "module"`
   *    is required under COEP: require-corp. */
  const cpuWorker   = new Worker(new URL("../workers/cpu.worker.ts",   import.meta.url), { type: "module" });
  const gpuWorker   = new Worker(new URL("../workers/gpu.worker.ts",   import.meta.url), { type: "module" });
  const audioWorker = new Worker(new URL("../workers/audio.worker.ts", import.meta.url), { type: "module" });

  type Slot = "pending" | "ready" | "failed";
  let cpuSlot:   Slot = "pending";
  let gpuSlot:   Slot = "pending";
  let audioSlot: Slot = "pending";

  let cpuBackend:   string | null = null;
  let adapterLabel: string | null = null;

  function slotGlyph(slot: Slot): string {
    return slot === "ready" ? "ok" : slot === "failed" ? "fail" : "…";
  }
  function updateStatus(): void {
    setStatus([
      `cpu=${slotGlyph(cpuSlot)}`,
      `gpu=${slotGlyph(gpuSlot)}`,
      `audio=${slotGlyph(audioSlot)}`,
    ].join(" · "));
  }
  updateStatus();

  const allReady = new Promise<void>((resolve) => {
    const maybeResolve = () => {
      if (cpuSlot !== "pending" && gpuSlot !== "pending" && audioSlot !== "pending") {
        resolve();
      }
    };

    attachWorkerLogRelay<CPUToMainMessage>("cpu", cpuWorker, msg => {
      if (msg.type === "ready") {
        cpuBackend = `${msg.backendName} v${msg.backendVersion}`;
        appendLogLine("info", `cpu backend: ${cpuBackend}`);
        cpuSlot = "ready";
        updateStatus();
        maybeResolve();
      } else if (msg.type === "halted") {
        appendLogLine("warn", "cpu halted");
      }
    }, () => { cpuSlot = "failed"; updateStatus(); maybeResolve(); });

    attachWorkerLogRelay<GPUToMainMessage>("gpu", gpuWorker, msg => {
      if (msg.type === "ready") {
        adapterLabel = msg.adapterName ?? "Software/Unknown";
        appendLogLine("info", `gpu adapter: ${adapterLabel}`);
        gpuSlot = "ready";
        updateStatus();
        maybeResolve();
      }
    }, () => { gpuSlot = "failed"; updateStatus(); maybeResolve(); });

    attachWorkerLogRelay<AudioToMainMessage>("audio", audioWorker, msg => {
      if (msg.type === "ready") {
        appendLogLine("info", "audio worker ready");
        audioSlot = "ready";
        updateStatus();
        maybeResolve();
      }
    }, () => { audioSlot = "failed"; updateStatus(); maybeResolve(); });

    window.setTimeout(() => {
      if (cpuSlot === "pending")   { cpuSlot   = "failed"; appendLogLine("error", `cpu worker did not report within ${WORKER_READY_TIMEOUT_MS}ms`); }
      if (gpuSlot === "pending")   { gpuSlot   = "failed"; appendLogLine("error", `gpu worker did not report within ${WORKER_READY_TIMEOUT_MS}ms`); }
      if (audioSlot === "pending") { audioSlot = "failed"; appendLogLine("error", `audio worker did not report within ${WORKER_READY_TIMEOUT_MS}ms`); }
      updateStatus();
      maybeResolve();
    }, WORKER_READY_TIMEOUT_MS);
  });

  const cpuInit:   MainToCPUMessage   = { type: "init", shared };
  const gpuInit:   MainToGPUMessage   = { type: "init", canvas: offscreen, shared };
  const audioInit: MainToAudioMessage = { type: "init", audioRing: shared.audioRing };

  cpuWorker.postMessage(cpuInit);
  gpuWorker.postMessage(gpuInit, [offscreen]);
  audioWorker.postMessage(audioInit);

  appendLogLine("info", "workers posted init messages");

  await allReady;

  return {
    adapterLabel: adapterLabel ?? "unavailable",
    cpuBackend:   cpuBackend   ?? "unavailable",
    ramMiB,
  };
}

boot()
  .then(async result => {
    if (!result) return;
    appendLogLine("info", "core online - mounting Solid shell");
    const { mountShell } = await import("./ui/mount");
    mountShell(result);
  })
  .catch(e => fatal(`unhandled boot error: ${(e as Error).message}`));
