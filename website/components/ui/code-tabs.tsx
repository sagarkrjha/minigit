"use client";

import { useState } from "react";
import { highlightCode } from "@/lib/prism";
import { FaCopy, FaCheck, FaJava } from "react-icons/fa6";
import {
  SiCplusplus,
  SiC,
  SiRust,
  SiPython,
  SiTypescript,
  SiJavascript,
  SiGo,
  SiDocker,
} from "react-icons/si";
import { FiCode } from "react-icons/fi";

export interface CodeTab {
  label: string;
  language: string;
  code: string;
}

function getTabIcon(label: string, language: string) {
  const normalized = (label + " " + language).toLowerCase();
  if (normalized.includes("c++") || normalized.includes("cpp")) {
    return <SiCplusplus className="h-4 w-4" />;
  }
  if (normalized.includes("rust")) {
    return <SiRust className="h-4 w-4" />;
  }
  if (normalized.includes("python") || normalized.includes("py")) {
    return <SiPython className="h-4 w-4" />;
  }
  if (normalized.includes("typescript") || normalized.includes("ts") || normalized.includes("node")) {
    return <SiTypescript className="h-4 w-4" />;
  }
  if (normalized.includes("javascript") || normalized.includes("js")) {
    return <SiJavascript className="h-4 w-4" />;
  }
  if (normalized.includes("go") || normalized.includes("golang")) {
    return <SiGo className="h-4 w-4" />;
  }
  if (normalized.includes("java")) {
    return <FaJava className="h-4 w-4" />;
  }
  if (normalized.includes("c ") || normalized.endsWith(" c") || normalized === "c") {
    return <SiC className="h-4 w-4" />;
  }
  if (normalized.includes("docker")) {
    return <SiDocker className="h-4 w-4" />;
  }
  return <FiCode className="h-4 w-4" />;
}

export function CodeTabs({ tabs }: { tabs: CodeTab[] }) {
  const [activeTab, setActiveTab] = useState(0);
  const [copied, setCopied] = useState(false);

  if (!tabs || tabs.length === 0) return null;

  const current = tabs[activeTab];

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(current.code);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch {
      // ignore
    }
  };

  const { highlightedHtml, language } = highlightCode(current.code, current.language);

  return (
    <div className="relative group my-6 overflow-hidden rounded-xl border border-border bg-zinc-50 dark:bg-[#07070a] shadow-xs dark:shadow-lg transition-colors">
      {/* Tab bar header */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-zinc-100/90 dark:bg-[#0e0e14] border-b border-border text-xs font-mono select-none overflow-x-auto">
        <div className="flex items-center gap-1.5">
          {tabs.map((tab, idx) => {
            const isActive = idx === activeTab;
            return (
              <button
                key={tab.label}
                type="button"
                onClick={() => {
                  setActiveTab(idx);
                  setCopied(false);
                }}
                title={tab.label}
                aria-label={tab.label}
                className={`p-2 rounded-lg transition-all duration-150 cursor-pointer flex items-center justify-center ${
                  isActive
                    ? "bg-primary text-primary-foreground font-semibold shadow-xs"
                    : "text-zinc-600 dark:text-zinc-400 hover:text-foreground dark:hover:text-zinc-200 hover:bg-zinc-200/60 dark:hover:bg-white/5"
                }`}
              >
                {getTabIcon(tab.label, tab.language)}
              </button>
            );
          })}
        </div>

        <button
          type="button"
          onClick={handleCopy}
          className="flex items-center gap-1.5 px-2.5 py-1 rounded-md text-[11px] font-mono text-zinc-600 dark:text-zinc-400 hover:text-foreground hover:bg-zinc-200/70 dark:hover:bg-white/10 transition-colors cursor-pointer shrink-0 ml-3"
        >
          {copied ? (
            <>
              <FaCheck className="h-3 w-3 text-emerald-600 dark:text-emerald-400" />
              <span className="text-emerald-600 dark:text-emerald-400 font-medium">Copied</span>
            </>
          ) : (
            <>
              <FaCopy className="h-3 w-3 text-zinc-600 dark:text-zinc-400" />
              <span>Copy</span>
            </>
          )}
        </button>
      </div>

      {/* Code window */}
      <pre
        suppressHydrationWarning
        className="overflow-x-auto p-4 font-mono text-[13px] leading-relaxed text-zinc-900 dark:text-zinc-200 bg-transparent"
      >
        <code
          suppressHydrationWarning
          className={`language-${language || current.language}`}
          // biome-ignore lint/security/noDangerouslySetInnerHtml: Prism output is sanitized HTML
          dangerouslySetInnerHTML={{
            __html: highlightedHtml || current.code.replace(/</g, "&lt;").replace(/>/g, "&gt;"),
          }}
        />
      </pre>
    </div>
  );
}
