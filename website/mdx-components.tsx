import type { MDXComponents } from "mdx/types";
import type { ComponentPropsWithoutRef } from "react";
import { highlightCode } from "@/lib/prism";

export function useMDXComponents(components?: MDXComponents): MDXComponents {
  return {
    h1: ({ className = "", ...props }: ComponentPropsWithoutRef<"h1">) => (
      <h1
        className={`scroll-m-20 text-3xl font-bold tracking-tight first:mt-0 ${className}`}
        {...props}
      />
    ),
    h2: ({ className = "", ...props }: ComponentPropsWithoutRef<"h2">) => (
      <h2
        className={`scroll-m-20 border-b border-border pb-2 text-2xl font-semibold tracking-tight first:mt-0 ${className}`}
        {...props}
      />
    ),
    h3: ({ className = "", ...props }: ComponentPropsWithoutRef<"h3">) => (
      <h3 className={`scroll-m-20 text-xl font-semibold tracking-tight ${className}`} {...props} />
    ),
    h4: ({ className = "", ...props }: ComponentPropsWithoutRef<"h4">) => (
      <h4 className={`scroll-m-20 text-lg font-semibold tracking-tight ${className}`} {...props} />
    ),
    p: ({ className = "", ...props }: ComponentPropsWithoutRef<"p">) => (
      <p className={`leading-7 not-first:mt-4 ${className}`} {...props} />
    ),
    ul: ({ className = "", ...props }: ComponentPropsWithoutRef<"ul">) => (
      <ul className={`my-4 ml-6 list-disc [&>li]:mt-2 ${className}`} {...props} />
    ),
    ol: ({ className = "", ...props }: ComponentPropsWithoutRef<"ol">) => (
      <ol className={`my-4 ml-6 list-decimal [&>li]:mt-2 ${className}`} {...props} />
    ),
    li: ({ className = "", ...props }: ComponentPropsWithoutRef<"li">) => (
      <li className={className} {...props} />
    ),
    blockquote: ({ className = "", ...props }: ComponentPropsWithoutRef<"blockquote">) => (
      <blockquote
        className={`mt-4 border-l-2 border-border pl-4 italic text-muted-foreground ${className}`}
        {...props}
      />
    ),
    code: ({ className = "", children, ...props }: ComponentPropsWithoutRef<"code">) => {
      const match = /language-([a-zA-Z0-9_-]+)/.exec(className || "");
      const rawLanguage = match ? match[1] : undefined;

      if (rawLanguage && typeof children === "string") {
        const { highlightedHtml, language } = highlightCode(children, rawLanguage);

        if (highlightedHtml) {
          return (
            <code
              className={`language-${language} font-mono text-sm block ${className}`}
              // biome-ignore lint/security/noDangerouslySetInnerHtml: Prism generates trusted syntax tokens
              dangerouslySetInnerHTML={{ __html: highlightedHtml }}
              {...props}
            />
          );
        }
      }

      return (
        <code
          className={`relative rounded bg-muted px-[0.3rem] py-[0.2rem] font-mono text-sm ${className}`}
          {...props}
        >
          {children}
        </code>
      );
    },
    pre: ({ className = "", ...props }: ComponentPropsWithoutRef<"pre">) => (
      <pre
        className={`mb-4 mt-4 overflow-x-auto rounded-lg border border-border bg-muted/60 p-4 font-mono text-sm leading-relaxed text-foreground [&>code]:bg-transparent [&>code]:p-0 [&>code]:rounded-none ${className}`}
        {...props}
      />
    ),
    hr: ({ className = "", ...props }: ComponentPropsWithoutRef<"hr">) => (
      <hr className={`my-6 border-border ${className}`} {...props} />
    ),
    a: ({ className = "", ...props }: ComponentPropsWithoutRef<"a">) => (
      <a
        className={`font-medium text-primary underline underline-offset-4 hover:opacity-80 ${className}`}
        {...props}
      />
    ),
    table: ({ className = "", ...props }: ComponentPropsWithoutRef<"table">) => (
      <div className="my-6 w-full overflow-x-auto rounded-lg border border-border shadow-xs">
        <table className={`w-full border-collapse text-left text-sm ${className}`} {...props} />
      </div>
    ),
    thead: ({ className = "", ...props }: ComponentPropsWithoutRef<"thead">) => (
      <thead className={`border-b border-border bg-muted/60 text-xs font-semibold uppercase tracking-wider text-muted-foreground ${className}`} {...props} />
    ),
    tbody: ({ className = "", ...props }: ComponentPropsWithoutRef<"tbody">) => (
      <tbody className={`divide-y divide-border ${className}`} {...props} />
    ),
    tr: ({ className = "", ...props }: ComponentPropsWithoutRef<"tr">) => (
      <tr className={`transition-colors hover:bg-muted/30 even:bg-muted/15 ${className}`} {...props} />
    ),
    th: ({ className = "", ...props }: ComponentPropsWithoutRef<"th">) => (
      <th className={`px-4 py-3 font-semibold text-foreground border-r border-border/50 last:border-r-0 ${className}`} {...props} />
    ),
    td: ({ className = "", ...props }: ComponentPropsWithoutRef<"td">) => (
      <td className={`px-4 py-3 text-muted-foreground border-r border-border/50 last:border-r-0 align-top ${className}`} {...props} />
    ),
    ...components,
  };
}
