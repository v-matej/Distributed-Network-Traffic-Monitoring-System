export type UiTheme = "dark" | "light";

const THEME_STORAGE_KEY = "dntm-ui-theme";

export function getStoredTheme(): UiTheme {
  if (typeof window === "undefined") {
    return "dark";
  }

  const storedTheme = window.localStorage.getItem(THEME_STORAGE_KEY);

  if (storedTheme === "light" || storedTheme === "dark") {
    return storedTheme;
  }

  return "dark";
}

export function applyTheme(theme: UiTheme) {
  if (typeof document === "undefined") {
    return;
  }

  document.documentElement.dataset.theme = theme;
}

export function setStoredTheme(theme: UiTheme) {
  if (typeof window !== "undefined") {
    window.localStorage.setItem(THEME_STORAGE_KEY, theme);
  }

  applyTheme(theme);
}

export function getThemeLabel(theme: UiTheme) {
  return theme === "light" ? "Light theme" : "Dark terminal theme";
}