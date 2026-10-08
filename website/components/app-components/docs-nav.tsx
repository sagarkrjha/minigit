"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { ThemeToggleButton } from "@/components/app-components";
import { Button } from "@/components/ui";
import { FaGithub } from "react-icons/fa6";
import { FiDownload, FiExternalLink } from "react-icons/fi";

export function DocsNavbar() {
  const pathname = usePathname();

  return (
    <header className="sticky top-0 z-50 w-full border-b border-border/80 bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
      <div className="max-w-7xl mx-auto flex h-14 items-center justify-between px-4 sm:px-8">
        <div className="flex items-center gap-6">
          <Link href="/" className="flex items-center gap-2.5 font-bold tracking-tight text-foreground">
            <span className="flex h-7 w-7 items-center justify-center rounded bg-primary text-primary-foreground font-mono text-sm font-semibold shadow-xs">
              μ
            </span>
            <span className="text-base font-bold tracking-tight">MiniGit</span>
            <span className="text-xs px-2 py-0.5 rounded-full bg-muted font-mono font-medium text-muted-foreground border border-border">
              v1.11.3
            </span>
          </Link>

          <nav className="hidden md:flex items-center gap-6 text-sm font-medium">
            <Link
              href="/docs/learn"
              className={pathname === "/docs/learn" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Learn
            </Link>
            <Link
              href="/docs/install"
              className={pathname === "/docs/install" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Install
            </Link>
            <Link
              href="/docs/reference"
              className={pathname === "/docs/reference" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Reference
            </Link>
            <Link
              href="/docs/about"
              className={pathname === "/docs/about" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              About
            </Link>
            <Link
              href="/docs/community"
              className={pathname === "/docs/community" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Community
            </Link>
          </nav>
        </div>

        <div className="flex items-center gap-3">
          <Button variant="default" size="sm" asChild className="hidden sm:inline-flex bg-primary text-primary-foreground hover:bg-primary/90 font-medium">
            <Link href="/docs/install">
              <FiDownload className="mr-1.5 h-3.5 w-3.5" /> Download
            </Link>
          </Button>
          <Button variant="outline" size="sm" asChild className="hidden sm:inline-flex text-xs">
            <a
              href="https://github.com/sagarkrjha/minigit"
              target="_blank"
              rel="noreferrer"
              className="flex items-center gap-1.5"
            >
              <FaGithub className="h-3.5 w-3.5" />
              <span>GitHub</span>
            </a>
          </Button>
          <ThemeToggleButton />
        </div>
      </div>
    </header>
  );
}

export function DocsSidebar() {
  const pathname = usePathname();

  return (
    <aside className="w-64 shrink-0 hidden md:block py-8 pr-6 border-r border-border min-h-[calc(100vh-3.5rem)] text-sm">
      <div className="space-y-6">
        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-2 px-3">
            Documentation
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs" current={pathname}>
              Overview
            </SidebarLink>
            <SidebarLink href="/docs/learn" current={pathname}>
              Learn (Concepts & Tutorial)
            </SidebarLink>
            <SidebarLink href="/docs/install" current={pathname}>
              Install (Binaries & Docker)
            </SidebarLink>
            <SidebarLink href="/docs/about" current={pathname}>
              About & License
            </SidebarLink>
            <SidebarLink href="/docs/community" current={pathname}>
              Community & Contributing
            </SidebarLink>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-2 px-3">
            Reference
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs/reference" current={pathname}>
              Index
            </SidebarLink>
            <SidebarLink href="/docs/reference/daily-workflow" current={pathname}>
              Daily Workflow
            </SidebarLink>
            <SidebarLink href="/docs/reference/branching" current={pathname}>
              Branching & Tags
            </SidebarLink>
            <SidebarLink href="/docs/reference/merging-rebase" current={pathname}>
              Merge & Rebase
            </SidebarLink>
            <SidebarLink href="/docs/reference/stash-worktree" current={pathname}>
              Stash & Worktree
            </SidebarLink>
            <SidebarLink href="/docs/reference/remotes" current={pathname}>
              Remotes & Network
            </SidebarLink>
            <SidebarLink href="/docs/reference/plumbing" current={pathname}>
              Plumbing Commands
            </SidebarLink>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-2 px-3">
            Ecosystem
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs/sdk-and-docker" current={pathname}>
              Polyglot SDK & Containers
            </SidebarLink>
            <SidebarLink href="https://github.com/sagarkrjha/minigit/releases" current={pathname}>
              Releases & Checksums ↗
            </SidebarLink>
          </div>
        </div>
      </div>
    </aside>
  );
}

function SidebarLink({
  href,
  current,
  children,
}: {
  href: string;
  current: string;
  children: React.ReactNode;
}) {
  const isExternal = href.startsWith("http");
  const isActive = current === href;

  if (isExternal) {
    return (
      <a
        href={href}
        target="_blank"
        rel="noreferrer"
        className="block px-3 py-1.5 rounded-md text-muted-foreground hover:bg-muted/60 hover:text-foreground transition-colors"
      >
        {children}
      </a>
    );
  }

  return (
    <Link
      href={href}
      className={`block px-3 py-1.5 rounded-md transition-colors ${
        isActive
          ? "bg-primary/10 text-primary font-semibold border-l-2 border-primary"
          : "text-muted-foreground hover:bg-muted/60 hover:text-foreground"
      }`}
    >
      {children}
    </Link>
  );
}

function SidebarAnchor({
  href,
  children,
}: {
  href: string;
  children: React.ReactNode;
}) {
  return (
    <Link
      href={href}
      className="block px-3 py-1.5 text-xs text-muted-foreground hover:bg-muted/60 hover:text-foreground transition-colors truncate"
    >
      {children}
    </Link>
  );
}
