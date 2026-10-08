import { DocsNavbar, DocsSidebar } from "@/components/app-components";

export default function DocsLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div className="min-h-screen flex flex-col bg-background">
      <DocsNavbar />
      <div className="flex-1 max-w-7xl w-full mx-auto px-4 sm:px-8 flex">
        <DocsSidebar />
        <main className="flex-1 min-w-0 py-8 md:pl-10 lg:pl-14 max-w-4xl">
          {children}
        </main>
      </div>
    </div>
  );
}
