import Prism from "prismjs";
import "prismjs/components/prism-javascript";
import "prismjs/components/prism-typescript";
import "prismjs/components/prism-jsx";
import "prismjs/components/prism-tsx";
import "prismjs/components/prism-bash";
import "prismjs/components/prism-json";
import "prismjs/components/prism-markdown";
import "prismjs/components/prism-python";
import "prismjs/components/prism-yaml";
import "prismjs/components/prism-sql";
import "prismjs/components/prism-diff";

const LANGUAGE_MAP: Record<string, string> = {
  ts: "typescript",
  typescript: "typescript",
  js: "javascript",
  javascript: "javascript",
  jsx: "jsx",
  tsx: "tsx",
  sh: "bash",
  bash: "bash",
  shell: "bash",
  zsh: "bash",
  json: "json",
  css: "css",
  html: "markup",
  markup: "markup",
  xml: "markup",
  svg: "markup",
  md: "markdown",
  markdown: "markdown",
  mdx: "markdown",
  py: "python",
  python: "python",
  yml: "yaml",
  yaml: "yaml",
  sql: "sql",
  diff: "diff",
};

export function highlightCode(
  code: string,
  rawLanguage?: string,
): { highlightedHtml: string | null; language: string } {
  if (!rawLanguage) {
    return { highlightedHtml: null, language: "" };
  }

  const language = LANGUAGE_MAP[rawLanguage.toLowerCase()] || rawLanguage.toLowerCase();
  const grammar = Prism.languages[language];

  if (!grammar) {
    return { highlightedHtml: null, language };
  }

  try {
    return {
      highlightedHtml: Prism.highlight(code, grammar, language),
      language,
    };
  } catch {
    return { highlightedHtml: null, language };
  }
}
