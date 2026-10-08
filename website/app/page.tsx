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
            MiniGit v1.11.3 • C++20 Core • Polyglot SDK • Docker Distribution
          </div>

          <h1 className="text-4xl sm:text-6xl font-extrabold tracking-tight text-foreground max-w-3xl mx-auto leading-tight">
            Distributed Version Control Re-Engineered from Scratch
          </h1>

          <p className="text-base sm:text-lg text-muted-foreground max-w-2xl mx-auto">
            A clean-room, Git-compatible version control system built in modern <strong>C++20</strong>. Complete with content-addressable storage, Eugene Myers' $O(ND)$ diffing, Smart HTTP v1 transfer protocol, polyglot SDKs, and native zero-dependency distribution.
          </p>

          <div className="flex flex-wrap items-center justify-center gap-3 pt-4">
            <Button size="lg" asChild>
              <Link href="/docs">Get Started →</Link>
            </Button>
            <Button size="lg" variant="outline" asChild>
              <Link href="/docs/sdk-and-docker">Explore Polyglot SDK</Link>
            </Button>
            <Button size="lg" variant="ghost" asChild>
              <a
                href="https://github.com/sagarkrjha/minigit"
                target="_blank"
                rel="noreferrer"
              >
                View on GitHub
              </a>
            </Button>
          </div>
        </div>
      </section>

      {/* Key Feature Cards */}
      <section className="py-16 md:py-24 bg-muted/20">
        <div className="max-w-7xl mx-auto px-4 sm:px-8">
          <div className="text-center max-w-2xl mx-auto mb-12">
            <h2 className="text-2xl sm:text-3xl font-bold tracking-tight text-foreground">
              Core Architecture & Highlights
            </h2>
            <p className="text-sm text-muted-foreground mt-2">
              Explore the engineering pillars behind MiniGit's speed, simplicity, and cryptographic safety.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  CAS
                </div>
                <CardTitle>Content-Addressable Storage</CardTitle>
                <CardDescription>
                  Immutable blobs, trees, commits, and tags addressed by OpenSSL SHA-256 with Packfile v2 delta compression.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/architecture" className="text-primary font-medium hover:underline">
                  Learn about CAS & Storage Internals →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  SDK
                </div>
                <CardTitle>Polyglot SDK Bindings</CardTitle>
                <CardDescription>
                  Embed MiniGit anywhere with native C++20, C FFI, Python, JavaScript / TypeScript, Go, Java, and Rust SDKs.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/sdk-and-docker" className="text-primary font-medium hover:underline">
                  Browse SDK APIs & Examples →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  OCI
                </div>
                <CardTitle>Docker Distribution</CardTitle>
                <CardDescription>
                  Multi-stage minimal container packaging CLI, SDK headers, static archive, and runtime dependencies.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/sdk-and-docker" className="text-primary font-medium hover:underline">
                  View Docker & Compose Setup →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  ALG
                </div>
                <CardTitle>Myers Diff & LCA Merge</CardTitle>
                <CardDescription>
                  Eugene Myers' $O(ND)$ greedy difference algorithm and BFS-based Lowest Common Ancestor commit graph traversal.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/algorithms" className="text-primary font-medium hover:underline">
                  Read Algorithmic Proofs →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  NET
                </div>
                <CardTitle>Git Smart HTTP Transport</CardTitle>
                <CardDescription>
                  Synchronize with GitHub, GitLab, and custom git daemons via 4-hex-length pkt-line protocol over libcurl.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/features" className="text-primary font-medium hover:underline">
                  Explore Network Remotes Spec →
                </Link>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <div className="h-8 w-8 rounded-md bg-primary/10 text-primary flex items-center justify-center font-mono font-bold text-sm mb-2">
                  CLI
                </div>
                <CardTitle>Full Porcelain & Plumbing</CardTitle>
                <CardDescription>
                  Interactive branch switching, rebasing, stash shelving, worktrees, submodules, and DAG binary search bisection.
                </CardDescription>
              </CardHeader>
              <CardContent className="text-xs text-muted-foreground">
                <Link href="/docs/cli-usage" className="text-primary font-medium hover:underline">
                  Check CLI Command Reference →
                </Link>
              </CardContent>
            </Card>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="mt-auto border-t border-border py-8 text-center text-xs text-muted-foreground">
        <p>
          MiniGit is licensed under the MIT License. Created by Sagar Kumar Jha.
        </p>
      </footer>
    </div>
  );
}
