"use client";

import { Moon02Icon, Sun02Icon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
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
        className="h-8 w-8 rounded-full border-2 border-foreground/20 bg-background"
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
      className="flex h-8 w-8 items-center justify-center rounded-full border-2 border-foreground/20 bg-background text-foreground transition-colors duration-300 hover:bg-foreground hover:text-background"
      title={isDark ? "Switch to light mode" : "Switch to dark mode"}
      aria-label={isDark ? "Switch to light mode" : "Switch to dark mode"}
    >
      <HugeiconsIcon
        icon={isDark ? Sun02Icon : Moon02Icon}
        size={20}
        strokeWidth={2}
        className="text-yellow-500"
      />
    </button>
  );
}

export default ThemeToggleButton;
