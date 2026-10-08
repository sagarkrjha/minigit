import { DocsNavbar, DocsSidebar, TableOfContents } from "@/components/app-components";

export default function DocsLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div className="min-h-screen flex flex-col bg-background">
      <DocsNavbar />
      <div className="flex-1 max-w-7xl w-full mx-auto px-4 sm:px-8 flex justify-between gap-8">
        <DocsSidebar />
        <main className="flex-1 min-w-0 py-8 md:px-4 max-w-4xl">
          {children}
        </main>
        <TableOfContents />
      </div>
    </div>
  );
}

