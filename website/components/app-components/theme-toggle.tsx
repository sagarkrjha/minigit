"use client";

import { FaMoon, FaSun } from "react-icons/fa6";
import { useTheme } from "next-themes";
import { useSyncExternalStore } from "react";

const emptySubscribe = () => () => {};

function ThemeToggleButton() {
  const { resolvedTheme, setTheme } = useTheme();
  const mounted = useSyncExternalStore(
    emptySubscribe,
    () => true,
    () => false,
  );

  if (!mounted) {
    return (
      <button
        type="button"
        className="h-8 w-8 rounded-full border border-border bg-background"
        aria-label="Toggle theme"
      />
    );
  }

  const isDark = resolvedTheme === "dark";

  const toggleTheme = () => {
    setTheme(isDark ? "light" : "dark");
  };

  return (
    <button
      type="button"
      onClick={toggleTheme}
      className="flex h-8 w-8 items-center justify-center rounded-lg border border-border bg-muted/60 text-foreground transition-colors hover:bg-muted hover:text-primary"
      title={isDark ? "Switch to light mode" : "Switch to dark mode"}
      aria-label={isDark ? "Switch to light mode" : "Switch to dark mode"}
    >
      {isDark ? (
        <FaSun className="h-4 w-4 text-amber-400 transition-transform duration-200 hover:rotate-45" />
      ) : (
        <FaMoon className="h-4 w-4 text-zinc-700 transition-transform duration-200 hover:-rotate-12" />
      )}
    </button>
  );
}

export default ThemeToggleButton;
