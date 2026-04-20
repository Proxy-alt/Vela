/**
 * platform/web/src/ui/App.tsx
 *
 * SolidJS shell mounted once the boot sequence reports all workers ready.
 * Phase 0 scope: a minimal landing surface that proves the reactive runtime
 * is alive and that the log/status streams keep flowing into the UI.
 * Phase 1+ will flesh this out into the game library + settings screens.
 */
import { For, createSignal, onCleanup, onMount } from "solid-js";
import type { LogEntry } from "../log";
import { getLogHistory, getStatus, subscribeLogs, subscribeStatus } from "../log";

interface AppProps {
  readonly adapterLabel: string;
  readonly cpuBackend:   string;
  readonly ramMiB:       number;
}

function App(props: AppProps) {
  const [logs, setLogs] = createSignal<readonly LogEntry[]>(getLogHistory().slice());
  const [status, setStatusSig] = createSignal(getStatus());

  onMount(() => {
    const offLogs = subscribeLogs((entry) => {
      setLogs((prev) => {
        const next = prev.length >= 500 ? prev.slice(1) : prev.slice();
        next.push(entry);
        return next;
      });
    });
    const offStatus = subscribeStatus((text) => setStatusSig(text));
    onCleanup(() => { offLogs(); offStatus(); });
  });

  return (
    <div class="vela-shell">
      <header class="vela-header">
        <div class="vela-brand">
          <span class="vela-title">Vela</span>
          <span class="vela-subtitle">Phase 0 · skeleton</span>
        </div>
        <div class="vela-status">{status()}</div>
      </header>

      <section class="vela-main">
        <div class="vela-hero">
          <h2>Runtime online.</h2>
          <p>All three workers initialised. The core is idle, waiting for a title.</p>
          <dl class="vela-facts">
            <div><dt>CPU backend</dt><dd>{props.cpuBackend}</dd></div>
            <div><dt>GPU adapter</dt><dd>{props.adapterLabel}</dd></div>
            <div><dt>Guest RAM</dt><dd>{props.ramMiB} MiB</dd></div>
          </dl>
        </div>

        <aside class="vela-log">
          <header>event log</header>
          <div class="vela-log-scroll">
            <For each={logs()}>
              {(entry) => (
                <p class={`log-line log-${entry.level}`}>
                  {`[${entry.level.toUpperCase().padEnd(5, " ")}] ${entry.message}`}
                </p>
              )}
            </For>
          </div>
        </aside>
      </section>
    </div>
  );
}

export default App;
