"use client";

import { useState } from "react";
import { FaCheck, FaCopy } from "react-icons/fa6";

export function CodeBlock({
  language,
  html,
  rawCode,
}: {
  language: string;
  html: string;
  rawCode: string;
}) {
  const [copied, setCopied] = useState(false);

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(rawCode);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch {
      // ignore
    }
  };

  return (
    <div className="relative group my-5 overflow-hidden rounded-xl border border-border bg-zinc-50 dark:bg-[#07070a] shadow-xs dark:shadow-lg transition-colors">
      <div className="flex items-center justify-between px-4 py-2 bg-zinc-100/90 dark:bg-[#0e0e14] border-b border-border text-xs font-mono text-muted-foreground select-none">
        <div className="flex items-center gap-2">
          <span className="h-2.5 w-2.5 rounded-full bg-rose-500/80 inline-block" />
          <span className="h-2.5 w-2.5 rounded-full bg-amber-500/80 inline-block" />
          <span className="h-2.5 w-2.5 rounded-full bg-emerald-500/80 inline-block" />
          <span className="ml-2 text-[11px] font-semibold text-zinc-600 dark:text-zinc-400 uppercase tracking-wider">
            {language || "code"}
          </span>
        </div>

        <button
          type="button"
          onClick={handleCopy}
          className="flex items-center gap-1.5 px-2 py-0.5 rounded text-[11px] font-mono text-zinc-600 dark:text-zinc-400 hover:text-foreground hover:bg-zinc-200/70 dark:hover:bg-white/10 transition-colors cursor-pointer"
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

      <pre
        suppressHydrationWarning
        className="overflow-x-auto p-4 font-mono text-[13px] leading-relaxed text-zinc-900 dark:text-zinc-200 bg-transparent"
      >
        <code
          suppressHydrationWarning
          className={`language-${language}`}
          // biome-ignore lint/security/noDangerouslySetInnerHtml: Prism output is sanitized HTML
          dangerouslySetInnerHTML={{ __html: html }}
        />
      </pre>
    </div>
  );
}
