/**
 * Main-thread log sink. During boot, mirrors every line into the #boot-log
 * panel so the Phase 0 success criterion ("logs visible") holds without
 * opening devtools. Additional subscribers (e.g. the Solid UI debug drawer)
 * can attach via `subscribeLogs`.
 */
import type { LogLevel } from "@bindings/protocol";

const MAX_LINES = 500;

export interface LogEntry {
  readonly level:     LogLevel;
  readonly message:   string;
  readonly timestamp: number;
}

const history: LogEntry[] = [];
const subscribers = new Set<(entry: LogEntry) => void>();

let statusText = "booting…";
const statusSubscribers = new Set<(text: string) => void>();

function bootLogContainer(): HTMLElement | null {
  return document.getElementById("boot-log");
}

function pushToBootPanel(entry: LogEntry): void {
  const container = bootLogContainer();
  if (!container) return;

  const line = `[${entry.level.toUpperCase().padEnd(5, " ")}] ${entry.message}`;
  const el = document.createElement("p");
  el.className = `log-line log-${entry.level}`;
  el.textContent = line;
  container.appendChild(el);

  while (container.childElementCount > MAX_LINES) {
    container.removeChild(container.firstElementChild!);
  }
  container.scrollTop = container.scrollHeight;
}

export function appendLogLine(level: LogLevel, message: string): void {
  const entry: LogEntry = { level, message, timestamp: Date.now() };

  const consoleFn =
    level === "error" ? console.error
    : level === "warn" ? console.warn
    : level === "info" ? console.info
    : console.debug;
  consoleFn(`[${level.toUpperCase().padEnd(5, " ")}] ${message}`);

  history.push(entry);
  if (history.length > MAX_LINES) history.shift();

  pushToBootPanel(entry);
  for (const fn of subscribers) fn(entry);
}

export function subscribeLogs(fn: (entry: LogEntry) => void): () => void {
  subscribers.add(fn);
  return () => subscribers.delete(fn);
}

export function getLogHistory(): readonly LogEntry[] {
  return history;
}

export function setStatus(text: string): void {
  statusText = text;
  for (const fn of statusSubscribers) fn(text);
}

export function getStatus(): string {
  return statusText;
}

export function subscribeStatus(fn: (text: string) => void): () => void {
  statusSubscribers.add(fn);
  return () => statusSubscribers.delete(fn);
}
