import type { MDXComponents } from "mdx/types";
import type { ComponentPropsWithoutRef } from "react";
import { highlightCode } from "@/lib/prism";
import {
  CodeBlock,
  CodeTabs,
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
  Separator,
  Alert,
  AlertDescription,
} from "@/components/ui";
import { SdkExampleTabs } from "@/components/app-components/sdk-example-tabs";
import { DockerSdkTabs } from "@/components/app-components/docker-sdk-tabs";
import { AutoDownloadButton, PlatformDetectorCard } from "@/components/app-components/auto-download";

function escapeHtml(str: string): string {
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

function slugify(children: React.ReactNode): string {
  if (typeof children === "string") {
    return children.toLowerCase().replace(/[^\w\s-]/g, "").trim().replace(/\s+/g, "-");
  }
  return "";
}

export function useMDXComponents(components?: MDXComponents): MDXComponents {
  return {
    h1: ({ className = "", id, children, ...props }: ComponentPropsWithoutRef<"h1">) => {
      const headingId = id || slugify(children);
      return (
        <h1
          id={headingId}
          className={`scroll-m-20 text-3xl font-bold tracking-tight text-foreground first:mt-0 ${className}`}
          {...props}
        >
          {children}
        </h1>
      );
    },
    h2: ({ className = "", id, children, ...props }: ComponentPropsWithoutRef<"h2">) => {
      const headingId = id || slugify(children);
      return (
        <h2
          id={headingId}
          className={`scroll-m-20 border-b border-border/80 pb-2 text-2xl font-semibold tracking-tight text-foreground first:mt-0 ${className}`}
          {...props}
        >
          {children}
        </h2>
      );
    },
    h3: ({ className = "", id, children, ...props }: ComponentPropsWithoutRef<"h3">) => {
      const headingId = id || slugify(children);
      return (
        <h3
          id={headingId}
          className={`scroll-m-20 text-xl font-semibold tracking-tight text-foreground ${className}`}
          {...props}
        >
          {children}
        </h3>
      );
    },
    h4: ({ className = "", id, children, ...props }: ComponentPropsWithoutRef<"h4">) => {
      const headingId = id || slugify(children);
      return (
        <h4
          id={headingId}
          className={`scroll-m-20 text-lg font-semibold tracking-tight text-foreground ${className}`}
          {...props}
        >
          {children}
        </h4>
      );
    },
    p: ({ className = "", ...props }: ComponentPropsWithoutRef<"p">) => (
      <p className={`leading-7 text-zinc-300 not-first:mt-4 ${className}`} {...props} />
    ),
    ul: ({ className = "", ...props }: ComponentPropsWithoutRef<"ul">) => (
      <ul className={`my-4 ml-6 list-disc text-zinc-300 [&>li]:mt-2 ${className}`} {...props} />
    ),
    ol: ({ className = "", ...props }: ComponentPropsWithoutRef<"ol">) => (
      <ol className={`my-4 ml-6 list-decimal text-zinc-300 [&>li]:mt-2 ${className}`} {...props} />
    ),
    li: ({ className = "", ...props }: ComponentPropsWithoutRef<"li">) => (
      <li className={className} {...props} />
    ),
    blockquote: ({ className = "", children }: ComponentPropsWithoutRef<"blockquote">) => (
      <Alert variant="note" className={`my-4 ${className}`}>
        <AlertDescription className="text-zinc-300 italic">{children}</AlertDescription>
      </Alert>
    ),
    hr: ({ className = "" }: ComponentPropsWithoutRef<"hr">) => (
      <Separator className={className} />
    ),
    a: ({ className = "", ...props }: ComponentPropsWithoutRef<"a">) => (
      <a
        className={`font-medium text-primary underline underline-offset-4 hover:text-primary/80 transition-colors ${className}`}
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
          className={`relative rounded-md bg-muted/80 border border-border/60 px-[0.35rem] py-[0.15rem] font-mono text-[13px] text-primary-foreground/90 font-medium ${className}`}
          {...props}
        >
          {children}
        </code>
      );
    },
    pre: ({ children }: ComponentPropsWithoutRef<"pre">) => {
      // In @next/mdx, <pre> wraps a <code> element with className="language-xyz" and text children
      if (
        children &&
        typeof children === "object" &&
        "props" in children &&
        children.props
      ) {
        const codeProps = children.props as ComponentPropsWithoutRef<"code">;
        const className = codeProps.className || "";
        const match = /language-([a-zA-Z0-9_-]+)/.exec(className);
        const rawLang = match ? match[1] : "";
        const rawCode =
          typeof codeProps.children === "string"
            ? codeProps.children
            : Array.isArray(codeProps.children)
            ? codeProps.children.join("")
            : "";

        if (rawCode) {
          const { highlightedHtml, language } = highlightCode(rawCode, rawLang);
          return (
            <CodeBlock
              language={language || rawLang}
              html={highlightedHtml || escapeHtml(rawCode)}
              rawCode={rawCode}
            />
          );
        }
      }

      return (
        <pre className="my-5 overflow-x-auto rounded-xl border border-border/80 bg-[#07070a] p-4 font-mono text-[13px] text-zinc-200">
          {children}
        </pre>
      );
    },
    table: ({ className = "", ...props }: ComponentPropsWithoutRef<"table">) => (
      <Table className={className} {...props} />
    ),
    thead: ({ className = "", ...props }: ComponentPropsWithoutRef<"thead">) => (
      <TableHeader className={className} {...props} />
    ),
    tbody: ({ className = "", ...props }: ComponentPropsWithoutRef<"tbody">) => (
      <TableBody className={className} {...props} />
    ),
    tr: ({ className = "", ...props }: ComponentPropsWithoutRef<"tr">) => (
      <TableRow className={className} {...props} />
    ),
    th: ({ className = "", ...props }: ComponentPropsWithoutRef<"th">) => (
      <TableHead className={className} {...props} />
    ),
    td: ({ className = "", ...props }: ComponentPropsWithoutRef<"td">) => (
      <TableCell className={className} {...props} />
    ),
    CodeTabs,
    SdkExampleTabs,
    DockerSdkTabs,
    AutoDownloadButton,
    PlatformDetectorCard,
    ...components,
  };
}
