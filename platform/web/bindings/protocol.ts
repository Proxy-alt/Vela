/**
 * Typed message protocol between the main thread and the workers. See
 * docs/DESIGN.md section 12 ("Worker message protocol"). Phase 0 ships
 * the minimum surface: init + log plumbing + halt. Loading games, gamepad
 * input, and save-state messages arrive in later phases.
 */

export interface SharedMemory {
  readonly guestRAM:  SharedArrayBuffer;
  readonly frameSync: SharedArrayBuffer;
  readonly audioRing: SharedArrayBuffer;
  readonly traceBuf:  SharedArrayBuffer;
}

export type MainToCPUMessage =
  | {
      readonly type: "init";
      readonly shared: SharedMemory;
    }
  | { readonly type: "halt" };

export type CPUToMainMessage =
  | { readonly type: "ready";  readonly backendName: string; readonly backendVersion: string }
  | { readonly type: "log";    readonly level: LogLevel;     readonly message: string        }
  | { readonly type: "halted" }
  | { readonly type: "error";  readonly message: string };

export type MainToGPUMessage =
  | {
      readonly type: "init";
      readonly canvas: OffscreenCanvas;
      readonly shared: SharedMemory;
    };

export type GPUToMainMessage =
  | { readonly type: "ready"; readonly adapterName: string | null }
  | { readonly type: "log";   readonly level: LogLevel; readonly message: string }
  | { readonly type: "error"; readonly message: string };

export type MainToAudioMessage =
  | { readonly type: "init"; readonly audioRing: SharedArrayBuffer };

export type AudioToMainMessage =
  | { readonly type: "ready" }
  | { readonly type: "log";   readonly level: LogLevel; readonly message: string }
  | { readonly type: "error"; readonly message: string };

export type LogLevel = "trace" | "debug" | "info" | "warn" | "error";
