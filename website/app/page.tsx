import Link from "next/link";
import { DocsNavbar } from "@/components/app-components";
import { Button, Card, CardHeader, CardTitle, CardDescription, CardContent } from "@/components/ui";

export default function HomePage() {
  return (
    <div className="min-h-screen flex flex-col bg-background">
      <DocsNavbar />

      {/* Hero Section */}
      <section className="relative overflow-hidden py-20 md:py-32 border-b border-border">
        <div className="max-w-5xl mx-auto px-4 sm:px-8 text-center space-y-6">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full border border-border bg-muted/60 text-xs font-mono text-muted-foreground">
            <span className="h-2 w-2 rounded-full bg-emerald-500 animate-pulse" />
            MiniGit v1.11.3 • Open Source • MIT License • Multi-Platform
          </div>

          <h1 className="text-4xl sm:text-6xl font-extrabold tracking-tight text-foreground max-w-3xl mx-auto leading-tight">
            Fast, Lightweight Git-Compatible Version Control
          </h1>

          <p className="text-base sm:text-lg text-muted-foreground max-w-2xl mx-auto">
            A standalone, zero-dependency version control tool and cross-language SDK built under the open source MIT License. Compatible with standard Git workflows and remotes.
          </p>

          <div className="flex flex-wrap items-center justify-center gap-4 pt-4">
            <Button size="lg" asChild className="bg-primary text-primary-foreground hover:bg-primary/90">
              <Link href="/docs/installation">Download Free v1.11.3</Link>
            </Button>
            <Button size="lg" variant="outline" asChild>
              <Link href="/docs/cli-usage">CLI Documentation</Link>
            </Button>
            <Button size="lg" variant="ghost" asChild>
              <Link href="/docs/sdk-and-docker">Polyglot SDK</Link>
            </Button>
          </div>
        </div>
      </section>

      {/* Feature Highlights */}
      <section className="py-16 md:py-24 bg-muted/20">
        <div className="max-w-7xl mx-auto px-4 sm:px-8">
          <div className="text-center max-w-2xl mx-auto mb-12">
            <h2 className="text-2xl sm:text-3xl font-bold tracking-tight text-foreground">
              Built for Modern Developer Workflows
            </h2>
            <p className="text-sm text-muted-foreground mt-2">
              Everything you need for version control, branch management, and programmatic automation.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  CLI
                </div>
                <CardTitle>Standard Git Workflows</CardTitle>
                <CardDescription>
                  Full daily workflow support: initialize repositories, stage changes, record commits, inspect logs, and manage branches.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/cli-usage" className="text-primary font-medium hover:underline">
                  View CLI User Guide →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  SDK
                </div>
                <CardTitle>Cross-Language SDK</CardTitle>
                <CardDescription>
                  Programmatic bindings for C++, C, Python, JavaScript / TypeScript, Go, Java, and Rust for seamless application embedding.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/sdk-and-docker" className="text-primary font-medium hover:underline">
                  Browse SDK Bindings & Examples →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  MIT
                </div>
                <CardTitle>MIT Open Source</CardTitle>
                <CardDescription>
                  100% free and open-source software. Unrestricted use for personal, academic, and commercial environments without lock-in.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/license" className="text-primary font-medium hover:underline">
                  Read MIT License Details →
                </Link>
              </CardContent>
            </Card>
          </div>
        </div>
      </section>

      {/* Direct Download Box */}
      <section className="py-16 border-t border-border bg-card">
        <div className="max-w-4xl mx-auto px-4 sm:px-8 text-center space-y-6">
          <h2 className="text-2xl sm:text-3xl font-bold tracking-tight text-foreground">
            Download MiniGit for Your Platform
          </h2>
          <p className="text-sm text-muted-foreground max-w-xl mx-auto">
            Standalone zero-dependency executables verified with cryptographic SHA-256 checksums.
          </p>

          <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 pt-4">
            <Card className="p-4 flex flex-col items-center justify-center text-center space-y-3">
              <span className="font-semibold text-sm">Windows</span>
              <span className="text-xs text-muted-foreground font-mono">minigit.exe (x86_64)</span>
              <Button size="sm" asChild className="w-full">
                <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe">
                  Download .exe
                </a>
              </Button>
            </Card>

            <Card className="p-4 flex flex-col items-center justify-center text-center space-y-3">
              <span className="font-semibold text-sm">Linux</span>
              <span className="text-xs text-muted-foreground font-mono">minigit-linux (x86_64)</span>
              <Button size="sm" asChild className="w-full">
                <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux">
                  Download Linux
                </a>
              </Button>
            </Card>

            <Card className="p-4 flex flex-col items-center justify-center text-center space-y-3">
              <span className="font-semibold text-sm">macOS</span>
              <span className="text-xs text-muted-foreground font-mono">minigit-macos (Apple Silicon)</span>
              <Button size="sm" asChild className="w-full">
                <a href="https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos">
                  Download macOS
                </a>
              </Button>
            </Card>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="mt-auto border-t border-border py-8 text-center text-xs text-muted-foreground">
        <p>
          MiniGit is free software licensed under the <Link href="/docs/license" className="underline hover:text-foreground">MIT License</Link>.
        </p>
      </footer>
    </div>
  );
}
