"use client";

import { useState } from "react";
import { highlightCode } from "@/lib/prism";
import { FaCopy, FaCheck } from "react-icons/fa6";

export interface CodeTab {
  label: string;
  language: string;
  code: string;
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
    <div className="relative group my-6 overflow-hidden rounded-xl border border-border/80 bg-[#07070a] shadow-lg">
      {/* Tab bar header */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-[#0e0e14] border-b border-border/60 text-xs font-mono select-none overflow-x-auto">
        <div className="flex items-center gap-1">
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
                className={`px-3 py-1.5 rounded-lg text-xs font-medium transition-all duration-150 cursor-pointer ${
                  isActive
                    ? "bg-primary text-primary-foreground font-semibold shadow-xs"
                    : "text-zinc-400 hover:text-zinc-200 hover:bg-white/5"
                }`}
              >
                {tab.label}
              </button>
            );
          })}
        </div>

        <button
          type="button"
          onClick={handleCopy}
          className="flex items-center gap-1.5 px-2.5 py-1 rounded-md text-[11px] font-mono text-zinc-400 hover:text-white hover:bg-white/10 transition-colors cursor-pointer shrink-0 ml-3"
        >
          {copied ? (
            <>
              <FaCheck className="h-3 w-3 text-emerald-400" />
              <span className="text-emerald-400">Copied</span>
            </>
          ) : (
            <>
              <FaCopy className="h-3 w-3 text-zinc-400" />
              <span>Copy</span>
            </>
          )}
        </button>
      </div>

      {/* Code window */}
      <pre
        suppressHydrationWarning
        className="overflow-x-auto p-4 font-mono text-[13px] leading-relaxed text-zinc-200"
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
