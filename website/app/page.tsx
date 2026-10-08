import Link from "next/link";
import { DocsNavbar } from "@/components/app-components";
import { Button, Card, CardHeader, CardTitle, CardDescription, CardContent } from "@/components/ui";

export default function HomePage() {
  return (
    <div className="min-h-screen flex flex-col bg-background selection:bg-primary/30">
      <DocsNavbar />

      {/* Main Git-SCM Style Hero Section */}
      <section className="relative overflow-hidden border-b border-border py-12 md:py-20 bg-linear-to-b from-card/60 to-background">
        <div className="max-w-6xl mx-auto px-4 sm:px-8">
          <div className="grid grid-cols-1 lg:grid-cols-12 gap-12 items-center">
            {/* Left Column: Title & Downloads (Git-scm style) */}
            <div className="lg:col-span-7 space-y-6">
              <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full border border-border bg-muted/60 text-xs font-mono text-muted-foreground">
                <span className="h-2 w-2 rounded-full bg-emerald-500 animate-pulse" />
                Latest Release: v1.11.3 • MIT Open Source
              </div>

              <div className="space-y-3">
                <h1 className="text-4xl sm:text-5xl md:text-6xl font-extrabold tracking-tight text-foreground">
                  --local --fast
                </h1>
                <p className="text-lg sm:text-xl text-primary font-semibold">
                  Fast, lightweight Git-compatible version control.
                </p>
                <p className="text-sm sm:text-base text-muted-foreground leading-relaxed max-w-xl">
                  MiniGit is a zero-dependency, open-source version control system and polyglot SDK built for modern developers. Everything you love about standard Git daily workflows, branching, merging, and automation.
                </p>
              </div>

              {/* Git-scm style Platform Download Callout */}
              <div className="p-5 rounded-xl border border-border bg-card/80 backdrop-blur-sm space-y-4">
                <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                  <div>
                    <h3 className="font-semibold text-base text-foreground">
                      Download for Windows, Linux & macOS
                    </h3>
                    <p className="text-xs text-muted-foreground mt-0.5">
                      Standalone executables & container distribution. No source compilation required.
                    </p>
                  </div>
                  <Button size="lg" asChild className="bg-primary text-primary-foreground hover:bg-primary/90 font-medium shrink-0 shadow-sm">
                    <Link href="/docs/installation">
                      Download v1.11.3
                    </Link>
                  </Button>
                </div>

                <div className="pt-2 border-t border-border/60 flex flex-wrap gap-4 text-xs font-mono text-muted-foreground">
                  <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe" className="hover:text-primary transition-colors">
                    ↓ minigit.exe (Windows)
                  </a>
                  <span className="text-border">•</span>
                  <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux" className="hover:text-primary transition-colors">
                    ↓ minigit-linux (Linux x86_64)
                  </a>
                  <span className="text-border">•</span>
                  <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos" className="hover:text-primary transition-colors">
                    ↓ minigit-macos (Apple Silicon)
                  </a>
                </div>
              </div>
            </div>

            {/* Right Column: Terminal Demonstration / Git Cheatsheet (Git-scm monitor style) */}
            <div className="lg:col-span-5">
              <div className="rounded-xl border border-border bg-[#050507] shadow-2xl overflow-hidden font-mono text-xs">
                <div className="px-4 py-3 bg-[#0a0a0e] border-b border-border flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <span className="h-3 w-3 rounded-full bg-rose-500/80 inline-block" />
                    <span className="h-3 w-3 rounded-full bg-amber-500/80 inline-block" />
                    <span className="h-3 w-3 rounded-full bg-emerald-500/80 inline-block" />
                  </div>
                  <span className="text-muted-foreground text-[11px]">minigit-terminal</span>
                </div>
                <div className="p-5 space-y-3 text-zinc-300 leading-relaxed overflow-x-auto">
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">minigit init</span>
                    <div className="text-emerald-400">Initialized empty mini_git repository</div>
                  </div>
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">minigit add .</span>
                  </div>
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">minigit commit -m &quot;Initial release&quot;</span>
                    <div className="text-zinc-400">[7e9a12c] Initial release</div>
                  </div>
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">minigit branch feature/payments</span>
                  </div>
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">minigit switch feature/payments</span>
                    <div className="text-primary">Switched to branch &apos;feature/payments&apos;</div>
                  </div>
                  <div>
                    <span className="text-muted-foreground">$ </span>
                    <span className="text-foreground font-semibold">docker run --rm -v $(pwd):/data minigit:latest status</span>
                    <div className="text-zinc-400">On branch feature/payments (working tree clean)</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* Git-SCM 3-Card Resource Pillars */}
      <section className="py-16 md:py-20 border-b border-border bg-card/20">
        <div className="max-w-6xl mx-auto px-4 sm:px-8">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            {/* Card 1: Reference Manual */}
            <Card className="flex flex-col justify-between border-border bg-card/60">
              <CardHeader className="space-y-3">
                <div className="h-10 w-10 rounded-lg bg-primary/10 text-primary flex items-center justify-center font-bold text-lg font-mono">
                  #_
                </div>
                <CardTitle className="text-xl">Reference Manual</CardTitle>
                <CardDescription className="text-sm leading-relaxed">
                  Comprehensive command-by-command guide covering everyday workflows, branching, merging, conflict resolution, rebase, and bisect.
                </CardDescription>
              </CardHeader>
              <CardContent className="pt-0">
                <Button variant="outline" size="sm" asChild className="w-full">
                  <Link href="/docs/cli-usage">Browse CLI Commands →</Link>
                </Button>
              </CardContent>
            </Card>

            {/* Card 2: Downloads & Containers */}
            <Card className="flex flex-col justify-between border-border bg-card/60">
              <CardHeader className="space-y-3">
                <div className="h-10 w-10 rounded-lg bg-primary/10 text-primary flex items-center justify-center font-bold text-lg font-mono">
                  📦
                </div>
                <CardTitle className="text-xl">Downloads & Containers</CardTitle>
                <CardDescription className="text-sm leading-relaxed">
                  Native pre-compiled executables for Windows, Linux, and macOS, alongside verified multi-stage Docker container images.
                </CardDescription>
              </CardHeader>
              <CardContent className="pt-0">
                <Button variant="outline" size="sm" asChild className="w-full">
                  <Link href="/docs/installation">Download Center →</Link>
                </Button>
              </CardContent>
            </Card>

            {/* Card 3: Polyglot SDK */}
            <Card className="flex flex-col justify-between border-border bg-card/60">
              <CardHeader className="space-y-3">
                <div className="h-10 w-10 rounded-lg bg-primary/10 text-primary flex items-center justify-center font-bold text-lg font-mono">
                  ⚡
                </div>
                <CardTitle className="text-xl">SDK & Cloud Sync</CardTitle>
                <CardDescription className="text-sm leading-relaxed">
                  Embed version control programmatically across C++, C, Python, JavaScript / TypeScript, Go, Java, and Rust applications.
                </CardDescription>
              </CardHeader>
              <CardContent className="pt-0">
                <Button variant="outline" size="sm" asChild className="w-full">
                  <Link href="/docs/sdk-and-docker">SDK Documentation →</Link>
                </Button>
              </CardContent>
            </Card>
          </div>
        </div>
      </section>

      {/* Official Git-scm Style Topic Grid */}
      <section className="py-16 md:py-20">
        <div className="max-w-6xl mx-auto px-4 sm:px-8 space-y-10">
          <div className="border-b border-border pb-4">
            <h2 className="text-2xl font-bold tracking-tight text-foreground">
              Essential Command Topics
            </h2>
            <p className="text-sm text-muted-foreground mt-1">
              Quick access to core operations modeled after standard Git conventions.
            </p>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-4 gap-6 text-sm">
            <div className="space-y-2">
              <h4 className="font-semibold text-foreground border-b border-border/60 pb-1">Setup & Init</h4>
              <ul className="space-y-1.5 text-muted-foreground">
                <li><Link href="/docs/installation" className="hover:text-primary">minigit install</Link></li>
                <li><Link href="/docs/cli-usage#initialize-a-repository-minigit-init" className="hover:text-primary">minigit init</Link></li>
                <li><Link href="/docs/cli-usage#clone-a-repository-minigit-clone" className="hover:text-primary">minigit clone</Link></li>
                <li><Link href="/docs/cli-usage#12-ignoring-files-minigitignore" className="hover:text-primary">.minigitignore</Link></li>
              </ul>
            </div>

            <div className="space-y-2">
              <h4 className="font-semibold text-foreground border-b border-border/60 pb-1">Basic Snapshotting</h4>
              <ul className="space-y-1.5 text-muted-foreground">
                <li><Link href="/docs/cli-usage#check-repository-status-minigit-status" className="hover:text-primary">minigit status</Link></li>
                <li><Link href="/docs/cli-usage#stage-changes-minigit-add" className="hover:text-primary">minigit add</Link></li>
                <li><Link href="/docs/cli-usage#commit-staged-changes-minigit-commit" className="hover:text-primary">minigit commit</Link></li>
                <li><Link href="/docs/cli-usage#inspect-differences-minigit-diff" className="hover:text-primary">minigit diff</Link></li>
              </ul>
            </div>

            <div className="space-y-2">
              <h4 className="font-semibold text-foreground border-b border-border/60 pb-1">Branch & Merge</h4>
              <ul className="space-y-1.5 text-muted-foreground">
                <li><Link href="/docs/cli-usage#list-branches-minigit-branch" className="hover:text-primary">minigit branch</Link></li>
                <li><Link href="/docs/cli-usage#switch-branches-minigit-switch" className="hover:text-primary">minigit switch</Link></li>
                <li><Link href="/docs/cli-usage#5-merging--conflict-resolution-minigit-merge" className="hover:text-primary">minigit merge</Link></li>
                <li><Link href="/docs/cli-usage#replay-linear-history-with-rebase-minigit-rebase" className="hover:text-primary">minigit rebase</Link></li>
              </ul>
            </div>

            <div className="space-y-2">
              <h4 className="font-semibold text-foreground border-b border-border/60 pb-1">History & Inspect</h4>
              <ul className="space-y-1.5 text-muted-foreground">
                <li><Link href="/docs/cli-usage#view-commit-history-minigit-log" className="hover:text-primary">minigit log</Link></li>
                <li><Link href="/docs/cli-usage#inspect-objects--commits-minigit-show" className="hover:text-primary">minigit show</Link></li>
                <li><Link href="/docs/cli-usage#7-shelving-work-with-stash-minigit-stash" className="hover:text-primary">minigit stash</Link></li>
                <li><Link href="/docs/cli-usage#undo-changes-with-reset-minigit-reset" className="hover:text-primary">minigit reset</Link></li>
              </ul>
            </div>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="mt-auto border-t border-border py-8 text-center text-xs text-muted-foreground bg-card/40">
        <p>
          MiniGit is distributed under the permissive <Link href="/docs/license" className="underline hover:text-foreground">MIT Open Source License</Link>.
        </p>
      </footer>
    </div>
  );
}

