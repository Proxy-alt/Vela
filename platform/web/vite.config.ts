import { defineConfig } from "vite";
import solid from "vite-plugin-solid";

/**
 * Cross-origin isolation is mandatory - SharedArrayBuffer is gated behind
 * COOP + COEP. See docs/DESIGN.md section 12 ("Required HTTP headers").
 * Vite's preview and dev servers both need these headers or the app fails
 * at boot with `!crossOriginIsolated`.
 */
const crossOriginIsolationHeaders = {
  "Cross-Origin-Opener-Policy":   "same-origin",
  "Cross-Origin-Embedder-Policy": "require-corp",
};

export default defineConfig({
  plugins: [solid()],
  server: {
    headers: crossOriginIsolationHeaders,
    port:    5173,
  },
  preview: {
    headers: crossOriginIsolationHeaders,
    port:    5174,
  },
  worker: {
    format: "es",
  },
  build: {
    target: "es2022",
    sourcemap: true,
  },
});
