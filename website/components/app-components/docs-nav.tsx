"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { ThemeToggleButton } from "@/components/app-components";
import { Button } from "@/components/ui";

export function DocsNavbar() {
  const pathname = usePathname();

  return (
    <header className="sticky top-0 z-50 w-full border-b border-border/60 bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
      <div className="max-w-7xl mx-auto flex h-14 items-center justify-between px-4 sm:px-8">
        <div className="flex items-center gap-6">
          <Link href="/" className="flex items-center gap-2 font-bold tracking-tight text-foreground">
            <span className="flex h-7 w-7 items-center justify-center rounded-lg bg-primary text-primary-foreground font-mono text-sm font-semibold">
              μ
            </span>
            <span>MiniGit</span>
            <span className="text-xs px-2 py-0.5 rounded-full bg-muted font-mono font-normal text-muted-foreground border border-border">
              v1.11.3
            </span>
          </Link>

          <nav className="hidden md:flex items-center gap-5 text-sm font-medium">
            <Link
              href="/docs"
              className={pathname === "/docs" ? "text-foreground font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Docs
            </Link>
            <Link
              href="/docs/installation"
              className={pathname === "/docs/installation" ? "text-foreground font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Downloads
            </Link>
            <Link
              href="/docs/cli-usage"
              className={pathname === "/docs/cli-usage" ? "text-foreground font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              CLI Guide
            </Link>
            <Link
              href="/docs/sdk-and-docker"
              className={pathname === "/docs/sdk-and-docker" ? "text-foreground font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              SDK & Docker
            </Link>
            <Link
              href="/docs/license"
              className={pathname === "/docs/license" ? "text-foreground font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              MIT License
            </Link>
          </nav>
        </div>

        <div className="flex items-center gap-3">
          <Button variant="default" size="sm" asChild className="hidden sm:inline-flex bg-primary text-primary-foreground hover:bg-primary/90">
            <Link href="/docs/installation">
              Download v1.11.3
            </Link>
          </Button>
          <Button variant="outline" size="sm" asChild className="hidden sm:inline-flex">
            <a
              href="https://github.com/sagarkrjha/minigit"
              target="_blank"
              rel="noreferrer"
            >
              GitHub
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
    <aside className="w-64 shrink-0 hidden md:block py-6 pr-6 border-r border-border min-h-[calc(100vh-3.5rem)]">
      <div className="space-y-6">
        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-3 px-3">
            Getting Started
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs" current={pathname}>
              Overview
            </SidebarLink>
            <SidebarLink href="/docs/installation" current={pathname}>
              Downloads & Setup
            </SidebarLink>
            <SidebarLink href="/docs/cli-usage" current={pathname}>
              CLI User Guide
            </SidebarLink>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-3 px-3">
            Developer SDK & Cloud
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs/sdk-and-docker" current={pathname}>
              Polyglot SDK & Docker
            </SidebarLink>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-3 px-3">
            Open Source
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs/license" current={pathname}>
              MIT License
            </SidebarLink>
            <SidebarLink href="https://github.com/sagarkrjha/minigit/issues" current={pathname}>
              Issue Tracker ↗
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
        className="block px-3 py-1.5 text-sm rounded-md text-muted-foreground hover:bg-muted/60 hover:text-foreground transition-colors"
      >
        {children}
      </a>
    );
  }

  return (
    <Link
      href={href}
      className={`block px-3 py-1.5 text-sm rounded-md transition-colors ${
        isActive
          ? "bg-primary/10 text-primary font-medium"
          : "text-muted-foreground hover:bg-muted/60 hover:text-foreground"
      }`}
    >
      {children}
    </Link>
  );
}
