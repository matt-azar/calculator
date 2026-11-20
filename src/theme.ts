const STORAGE_KEY = "calculator-light";

export type ChromeSettings = {
  lightMode: boolean;
};

export function readChromeSettings(): ChromeSettings {
  return {
    lightMode: localStorage.getItem(STORAGE_KEY) === "on",
  };
}

export function writeChromeSettings(chrome: ChromeSettings): void {
  localStorage.setItem(STORAGE_KEY, chrome.lightMode ? "on" : "off");
}

export function applyTheme(chrome: ChromeSettings): void {
  document.documentElement.dataset.theme = chrome.lightMode ? "light" : "dark";
}

export function terminalTheme(lightMode: boolean) {
  if (lightMode) {
    return {
      background: "#f5f5f5",
      foreground: "#1e1e1e",
      cursor: "#1e1e1e",
      cursorAccent: "#f5f5f5",
      selectionBackground: "#add6ff",
      selectionForeground: "#1e1e1e",
      black: "#1e1e1e",
      red: "#c42b1c",
      green: "#0e7a0d",
      yellow: "#9a6700",
      blue: "#0451a5",
      magenta: "#9b59b6",
      cyan: "#098658",
      white: "#5a5a5a",
      brightBlack: "#8a8a8a",
      brightRed: "#e81123",
      brightGreen: "#16a34a",
      brightYellow: "#ca8a04",
      brightBlue: "#0078d4",
      brightMagenta: "#c026d3",
      brightCyan: "#0d9488",
      brightWhite: "#1e1e1e",
    };
  }
  return {
    background: "#181818",
    foreground: "#c8c8c8",
    cursor: "#c8c8c8",
    cursorAccent: "#181818",
    selectionBackground: "#264f78",
    selectionForeground: "#c8c8c8",
    black: "#181818",
    red: "#e06c75",
    green: "#98c379",
    yellow: "#e5c07b",
    blue: "#61afef",
    magenta: "#c678dd",
    cyan: "#56b6c2",
    white: "#c8c8c8",
    brightBlack: "#6e6e6e",
    brightRed: "#e06c75",
    brightGreen: "#98c379",
    brightYellow: "#e5c07b",
    brightBlue: "#61afef",
    brightMagenta: "#c678dd",
    brightCyan: "#56b6c2",
    brightWhite: "#ffffff",
  };
}
