"use client";

import { useEffect, useState } from "react";
import { usePathname } from "next/navigation";

interface TocItem {
  id: string;
  text: string;
  level: number;
}

export function TableOfContents() {
  const pathname = usePathname();
  const [headings, setHeadings] = useState<TocItem[]>([]);
  const [activeId, setActiveId] = useState<string>("");

  useEffect(() => {
    // Run after DOM has painted the MDX page content
    const scanHeadings = () => {
      const elements = Array.from(
        document.querySelectorAll("main h2, main h3")
      ) as HTMLElement[];

      const items: TocItem[] = [];

      elements.forEach((el) => {
        if (!el.id) {
          const generatedId = (el.textContent || "")
            .toLowerCase()
            .replace(/[^\w\s-]/g, "")
            .trim()
            .replace(/\s+/g, "-");
          if (generatedId) {
            el.id = generatedId;
          }
        }

        if (el.id && el.textContent) {
          items.push({
            id: el.id,
            text: el.textContent.replace(/^[#\s]+/, "").trim(),
            level: el.tagName === "H2" ? 2 : 3,
          });
        }
      });

      setHeadings(items);

      if (items.length > 0 && !activeId) {
        setActiveId(items[0].id);
      }
    };

    // Initial scan and small delay scan to account for dynamic hydration
    scanHeadings();
    const timer = setTimeout(scanHeadings, 150);

    const handleScroll = () => {
      const elements = Array.from(
        document.querySelectorAll("main h2, main h3")
      ) as HTMLElement[];
      if (elements.length === 0) return;

      const scrollPosition = window.scrollY + 120;
      let currentActive = elements[0]?.id || "";

      for (let i = 0; i < elements.length; i++) {
        const el = elements[i];
        if (el.offsetTop <= scrollPosition) {
          currentActive = el.id;
        } else {
          break;
        }
      }

      if (currentActive) {
        setActiveId(currentActive);
      }
    };

    window.addEventListener("scroll", handleScroll, { passive: true });
    // Run once on mount
    handleScroll();

    return () => {
      clearTimeout(timer);
      window.removeEventListener("scroll", handleScroll);
    };
  }, [pathname]);

  if (headings.length === 0) {
    return null;
  }

  return (
    <aside className="hidden xl:block w-64 shrink-0 py-8 pl-6 border-l border-border/80 sticky top-14 h-[calc(100vh-3.5rem)] overflow-y-auto select-none">
      <div className="space-y-4">
        <div className="flex items-center gap-2 px-2.5">
          <span className="relative flex h-2 w-2">
            <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-primary opacity-75" />
            <span className="relative inline-flex rounded-full h-2 w-2 bg-primary" />
          </span>
          <h4 className="text-[11px] font-bold uppercase tracking-wider text-muted-foreground/90 font-mono">
            On this page
          </h4>
        </div>

        <nav className="relative space-y-1 text-xs">
          {headings.map((heading) => {
            const isActive = activeId === heading.id;

            return (
              <a
                key={heading.id}
                href={`#${heading.id}`}
                onClick={(e) => {
                  e.preventDefault();
                  const target = document.getElementById(heading.id);
                  if (target) {
                    const y = target.getBoundingClientRect().top + window.pageYOffset - 80;
                    window.scrollTo({ top: y, behavior: "smooth" });
                    setActiveId(heading.id);
                    window.history.pushState(null, "", `#${heading.id}`);
                  }
                }}
                className={`group relative flex items-center transition-all duration-200 py-1.5 px-3 rounded-md leading-snug ${
                  heading.level === 3 ? "pl-5 text-[11px] text-muted-foreground/80" : "font-medium"
                } ${
                  isActive
                    ? "text-primary font-semibold bg-primary/10 border-l-2 border-primary"
                    : "text-muted-foreground hover:text-foreground hover:bg-muted/40"
                }`}
              >
                <span className="truncate">{heading.text}</span>
              </a>
            );
          })}
        </nav>
      </div>
    </aside>
  );
}
