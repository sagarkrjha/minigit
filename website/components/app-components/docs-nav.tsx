"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { ThemeToggleButton } from "@/components/app-components";
import { Button } from "@/components/ui";

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
              href="/docs"
              className={pathname === "/docs" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Documentation
            </Link>
            <Link
              href="/docs/cli-usage"
              className={pathname === "/docs/cli-usage" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Reference Manual
            </Link>
            <Link
              href="/docs/installation"
              className={pathname === "/docs/installation" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              Downloads
            </Link>
            <Link
              href="/docs/sdk-and-docker"
              className={pathname === "/docs/sdk-and-docker" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              SDK & Docker
            </Link>
            <Link
              href="/docs/license"
              className={pathname === "/docs/license" ? "text-primary font-semibold" : "text-muted-foreground hover:text-foreground transition-colors"}
            >
              MIT License
            </Link>
          </nav>
        </div>

        <div className="flex items-center gap-3">
          <Button variant="default" size="sm" asChild className="hidden sm:inline-flex bg-primary text-primary-foreground hover:bg-primary/90 font-medium">
            <Link href="/docs/installation">
              Download v1.11.3
            </Link>
          </Button>
          <Button variant="outline" size="sm" asChild className="hidden sm:inline-flex text-xs">
            <a
              href="https://github.com/sagarkrjha/minigit"
              target="_blank"
              rel="noreferrer"
            >
              GitHub ↗
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
            Reference & Guides
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs" current={pathname}>
              Overview & Topics
            </SidebarLink>
            <SidebarLink href="/docs/cli-usage" current={pathname}>
              CLI Reference Manual
            </SidebarLink>
            <SidebarLink href="/docs/installation" current={pathname}>
              Downloads & Install
            </SidebarLink>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-2 px-3">
            Core CLI Topics
          </h4>
          <div className="space-y-1">
            <SidebarAnchor href="/docs/cli-usage#2-daily-development-workflow">
              Workflows (init, add, commit)
            </SidebarAnchor>
            <SidebarAnchor href="/docs/cli-usage#3-branching--switching">
              Branching & Switching
            </SidebarAnchor>
            <SidebarAnchor href="/docs/cli-usage#5-merging--conflict-resolution-minigit-merge">
              Merging & Conflicts
            </SidebarAnchor>
            <SidebarAnchor href="/docs/cli-usage#6-undoing--history-rewriting">
              Rebase, Reset & Revert
            </SidebarAnchor>
            <SidebarAnchor href="/docs/cli-usage#7-shelving-work-with-stash-minigit-stash">
              Stash & Shelving
            </SidebarAnchor>
            <SidebarAnchor href="/docs/cli-usage#11-remote-repositories--synchronization">
              Remotes & Network
            </SidebarAnchor>
          </div>
        </div>

        <div>
          <h4 className="font-semibold text-xs uppercase tracking-wider text-muted-foreground mb-2 px-3">
            Ecosystem & Cloud
          </h4>
          <div className="space-y-1">
            <SidebarLink href="/docs/sdk-and-docker" current={pathname}>
              Polyglot SDK & Docker
            </SidebarLink>
            <SidebarLink href="/docs/license" current={pathname}>
              MIT License
            </SidebarLink>
            <SidebarLink href="https://github.com/sagarkrjha/minigit/releases" current={pathname}>
              Release Assets & Hashes ↗
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
