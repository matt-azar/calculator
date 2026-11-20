import { getCurrentWindow } from "@tauri-apps/api/window";
import {
  applyTheme,
  readChromeSettings,
  writeChromeSettings,
  type ChromeSettings,
} from "./theme";
import { createTerminal, type CalculatorTerminal } from "./terminal";

function inTauri(): boolean {
  return typeof window !== "undefined" && "__TAURI_INTERNALS__" in window;
}

const chrome: ChromeSettings = readChromeSettings();
applyTheme(chrome);

const menubar = document.querySelector("#menubar") as HTMLElement;
const maxButton = document.querySelector("#win-max") as HTMLButtonElement;
const terminalHost = document.querySelector("#terminal-host") as HTMLElement;

let terminal: CalculatorTerminal | null = null;
let openMenu: HTMLElement | null = null;

type MenuItem =
  | { kind: "action"; label: string; accel?: string; enabled?: boolean; checked?: boolean; run: () => void }
  | { kind: "sep" };

function closeMenus(): void {
  openMenu?.remove();
  openMenu = null;
  for (const btn of Array.from(menubar.querySelectorAll(".menu-button"))) {
    btn.setAttribute("aria-expanded", "false");
  }
}

function showMenu(anchor: HTMLElement, items: MenuItem[]): void {
  closeMenus();
  const menu = document.createElement("div");
  menu.className = "menu";
  menu.setAttribute("role", "menu");
  for (const item of items) {
    if (item.kind === "sep") {
      menu.appendChild(document.createElement("hr"));
      continue;
    }
    const button = document.createElement("button");
    button.type = "button";
    button.setAttribute("role", "menuitem");
    if (item.enabled === false) button.disabled = true;
    const check = document.createElement("span");
    check.className = item.checked ? "check on" : "check";
    button.appendChild(check);
    button.appendChild(document.createTextNode(item.label));
    if (item.accel) {
      const hint = document.createElement("span");
      hint.className = "hint";
      hint.textContent = item.accel;
      button.appendChild(hint);
    }
    button.addEventListener("click", () => {
      closeMenus();
      item.run();
    });
    menu.appendChild(button);
  }
  document.body.appendChild(menu);
  const rect = anchor.getBoundingClientRect();
  menu.style.left = `${Math.max(4, rect.left)}px`;
  menu.style.top = `${rect.bottom}px`;
  openMenu = menu;
  anchor.setAttribute("aria-expanded", "true");
}

function toggleLightMode(): void {
  chrome.lightMode = !chrome.lightMode;
  writeChromeSettings(chrome);
  applyTheme(chrome);
  terminal?.setLightMode(chrome.lightMode);
}

function viewMenuItems(): MenuItem[] {
  return [
    {
      kind: "action",
      label: "Light Mode",
      accel: "M",
      checked: chrome.lightMode,
      run: toggleLightMode,
    },
  ];
}

function helpMenuItems(): MenuItem[] {
  return [
    {
      kind: "action",
      label: "About Calculator",
      run: () => {
        window.alert(
          "Calculator\nIPython console with math, cmath, and statistics preloaded.\nTry: sin(pi/2)",
        );
      },
    },
  ];
}

function buildMenus(): void {
  menubar.replaceChildren();
  for (const [label, items] of [
    ["View", viewMenuItems],
    ["Help", helpMenuItems],
  ] as const) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "menu-button";
    button.textContent = label;
    button.setAttribute("aria-haspopup", "true");
    button.setAttribute("aria-expanded", "false");
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      if (openMenu && button.getAttribute("aria-expanded") === "true") {
        closeMenus();
        return;
      }
      showMenu(button, items());
    });
    menubar.appendChild(button);
  }
}

async function refreshMaximize(): Promise<void> {
  if (!inTauri()) return;
  const maximized = await getCurrentWindow().isMaximized();
  maxButton.setAttribute("aria-label", maximized ? "Restore" : "Maximize");
}

function wireWindowControls(): void {
  document.querySelector("#win-min")?.addEventListener("click", () => {
    if (inTauri()) void getCurrentWindow().minimize();
  });
  maxButton.addEventListener("click", () => {
    if (!inTauri()) return;
    void getCurrentWindow().toggleMaximize().then(() => refreshMaximize());
  });
  document.querySelector("#win-close")?.addEventListener("click", () => {
    if (inTauri()) void getCurrentWindow().close();
  });

  for (const el of Array.from(document.querySelectorAll("[data-tauri-drag-region]"))) {
    el.addEventListener("mousedown", (event: Event) => {
      if (!inTauri()) return;
      if ((event as MouseEvent).button !== 0) return;
      void getCurrentWindow().startDragging();
    });
  }
}

function wireShortcuts(): void {
  window.addEventListener("keydown", (event) => {
    if (!(event.altKey || event.metaKey) && event.ctrlKey && event.key.toLowerCase() === "m") {
      event.preventDefault();
      toggleLightMode();
      return;
    }
    if (event.key === "Escape") closeMenus();
  });
  document.addEventListener("click", () => closeMenus());
}

async function boot(): Promise<void> {
  buildMenus();
  wireWindowControls();
  wireShortcuts();
  await refreshMaximize();

  if (inTauri()) {
    terminal = await createTerminal(terminalHost, chrome.lightMode);
    await getCurrentWindow().show();
  } else {
    terminalHost.textContent =
      "Open with `npm run tauri -- dev` to attach a live IPython session.";
    terminalHost.style.padding = "24px";
    terminalHost.style.color = "var(--muted)";
  }
}

void boot();
