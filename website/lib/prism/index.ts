import Prism from "prismjs";

// Prevent Prism from automatically querying and modifying client DOM elements (which adds language-* and tabindex to <pre>)
Prism.manual = true;

// Import all languages used in MiniGit docs
import "prismjs/components/prism-c";
import "prismjs/components/prism-cpp";
import "prismjs/components/prism-rust";
import "prismjs/components/prism-go";
import "prismjs/components/prism-java";
import "prismjs/components/prism-python";
import "prismjs/components/prism-javascript";
import "prismjs/components/prism-typescript";
import "prismjs/components/prism-jsx";
import "prismjs/components/prism-tsx";
import "prismjs/components/prism-bash";
import "prismjs/components/prism-powershell";
import "prismjs/components/prism-json";
import "prismjs/components/prism-markdown";
import "prismjs/components/prism-yaml";
import "prismjs/components/prism-sql";
import "prismjs/components/prism-diff";
import "prismjs/components/prism-cmake";
import "prismjs/components/prism-docker";

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
  powershell: "powershell",
  ps1: "powershell",
  posh: "powershell",
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
  cpp: "cpp",
  "c++": "cpp",
  c: "c",
  h: "c",
  hpp: "cpp",
  rust: "rust",
  rs: "rust",
  go: "go",
  golang: "go",
  java: "java",
  yml: "yaml",
  yaml: "yaml",
  sql: "sql",
  diff: "diff",
  patch: "diff",
  docker: "docker",
  dockerfile: "docker",
  cmake: "cmake",
};

export function highlightCode(
  code: string,
  rawLanguage?: string,
): { highlightedHtml: string | null; language: string } {
  if (!rawLanguage) {
    return { highlightedHtml: null, language: "" };
  }

  const normalized = rawLanguage.toLowerCase().trim();
  const language = LANGUAGE_MAP[normalized] || normalized;
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
