import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import "@xterm/xterm/css/xterm.css";
import { terminalTheme } from "./theme";

export type CalculatorTerminal = {
  term: Terminal;
  fit: FitAddon;
  dispose: () => void;
  setLightMode: (light: boolean) => void;
};

function decodeBase64(data: string): Uint8Array {
  const binary = atob(data);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function encodeBase64(data: string): string {
  const bytes = new TextEncoder().encode(data);
  let binary = "";
  for (let i = 0; i < bytes.length; i += 1) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

export async function createTerminal(
  host: HTMLElement,
  lightMode: boolean,
): Promise<CalculatorTerminal> {
  const term = new Terminal({
    cursorBlink: true,
    convertEol: true,
    fontFamily: '"Cascadia Mono", "Consolas", "Courier New", monospace',
    fontSize: 14,
    lineHeight: 1.25,
    theme: terminalTheme(lightMode),
    allowProposedApi: true,
  });
  const fit = new FitAddon();
  term.loadAddon(fit);
  term.open(host);
  fit.fit();

  const unlisteners: UnlistenFn[] = [];

  unlisteners.push(
    await listen<string>("pty-data", (event) => {
      term.write(decodeBase64(event.payload));
    }),
  );

  unlisteners.push(
    await listen<string>("pty-exit", (event) => {
      term.writeln(`\r\n[IPython exited: ${event.payload}]`);
    }),
  );

  unlisteners.push(
    await listen<string>("pty-error", (event) => {
      term.writeln(`\r\n[error] ${event.payload}`);
    }),
  );

  term.onData((data) => {
    void invoke("pty_write", { data: encodeBase64(data) });
  });

  const resize = () => {
    fit.fit();
    void invoke("pty_resize", {
      cols: term.cols,
      rows: term.rows,
    });
  };

  const observer = new ResizeObserver(() => resize());
  observer.observe(host);

  await invoke("pty_start", {
    cols: term.cols,
    rows: term.rows,
  });
  resize();
  term.focus();

  return {
    term,
    fit,
    setLightMode(light: boolean) {
      term.options.theme = terminalTheme(light);
    },
    dispose() {
      observer.disconnect();
      for (const stop of unlisteners) stop();
      term.dispose();
      void invoke("pty_stop");
    },
  };
}
