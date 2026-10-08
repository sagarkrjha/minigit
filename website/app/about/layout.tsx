import type { Metadata } from "next";

export const metadata: Metadata = {
  title: "About",
  description: "Demonstration of MDX with Next.js App Router",
};

export default function AboutLayout({ children }: { children: React.ReactNode }) {
  return <div className="mx-auto max-w-2xl px-6 py-12">{children}</div>;
}
