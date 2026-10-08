# Next.js Starter Template

A modern, production-ready, reusable starter template built with **Next.js 16 (App Router)**, **React 19**, **Tailwind CSS v4**, **shadcn/ui**, **Radix UI**, **next-themes**, and **Hugeicons**.

---

## ⚡ Tech Stack

- **Framework**: [Next.js 16](https://nextjs.org/) (App Router, Turbopack)
- **Content / MDX**: [@next/mdx](https://nextjs.org/docs/app/building-your-application/configuring/mdx)
- **Library**: [React 19](https://react.dev/)
- **Styling**: [Tailwind CSS v4](https://tailwindcss.com/)
- **Components**: [shadcn/ui](https://ui.shadcn.com/) (`radix-mira` style) & [Radix UI](https://www.radix-ui.com/)
- **Icons**: [Hugeicons React](https://hugeicons.com/)
- **Theme Support**: [next-themes](https://github.com/pacocoursey/next-themes) (Light / Dark mode toggle out of the box)
- **Type Safety**: [TypeScript 5](https://www.typescriptlang.org/)
- **Linter & Formatter**: [Biome](https://biomejs.dev/)

---

## 🚀 Features

- 📝 **Markdown & MDX Support**: First-class `@next/mdx` support. Write `.md` or `.mdx` pages directly with custom themed HTML component styling and interactive React components.
- 🌓 **Dark & Light Mode**: Pre-configured persistent theme support via `next-themes` and a ready-to-use toggle button.
- 🎨 **Tailwind CSS v4 + shadcn/ui**: Modern theme token configuration (`oklch` color spaces, CSS variables, container queries).
- 🧩 **UI Component Library**: Pre-wired `Button`, `Card`, and theme toggle components with barrel exports.
- 🗂 **Clean Architecture**: Structured folders for reusable UI components (`components/ui`), app components (`components/app-components`), providers (`components/providers`), and utilities (`lib/utils`).
- ⚡ **Turbopack**: Ultra-fast local development and build times.

---

## 📁 Project Structure

```text
├── app/
│   ├── about/
│   │   └── page.mdx         # Example MDX page demonstrating React components in markdown
│   ├── favicon.ico
│   ├── globals.css          # Tailwind CSS v4 setup & theme design tokens (oklch)
│   ├── layout.tsx           # Root layout with font configuration & ThemeProvider
│   └── page.tsx             # Starter landing page showcasing components
├── components/
│   ├── app-components/      # Application-specific composite components
│   │   ├── theme-toggle.tsx # Light/dark mode toggle button with Hugeicons
│   │   └── index.tsx        # Barrel export
│   ├── providers/           # Context providers (next-themes)
│   │   ├── theme-provider.tsx
│   │   └── index.tsx        # Barrel export
│   └── ui/                  # Reusable shadcn/ui primitive components
│       ├── button.tsx       # Button component with variants & sizes
│       ├── card.tsx         # Card header/content/footer/action components
│       └── index.tsx        # Barrel export
├── lib/
│   └── utils/               # Helper utilities (cn class merging)
│       └── index.ts
├── public/                  # Static assets
├── components.json          # shadcn/ui CLI configuration
├── biome.json               # Biome linter, formatter, and import sorter config
├── mdx-components.tsx       # Custom MDX component styling (headings, code blocks, lists)
├── next.config.ts           # Next.js configuration with @next/mdx
├── package.json
└── tsconfig.json
```

---

## 📋 Prerequisites

Before installing, ensure you have the following installed on your machine:

- **Node.js**: `20.x` or later (LTS recommended)
- **Package Manager**: [pnpm](https://pnpm.io/) (`v12.x` recommended, specified in `package.json`), or `npm`, `yarn`, or `bun`.

Check your versions:
```bash
node -v
pnpm -v
```

---

## 📥 Installation

### 1. Clone the Repository

Clone the project to your local machine:

```bash
git clone <your-repository-url> my-app
cd my-app
```

Or click **"Use this template"** on GitHub to create a new repository from this starter.

### 2. Install Dependencies

Install the required dependencies using your preferred package manager:

```bash
# Using pnpm (recommended)
pnpm install

# Using npm
npm install

# Using yarn
yarn install

# Using bun
bun install
```

> **Note**: No additional `.env` setup is required to get started. You can create a `.env.local` file when you need project-specific environment variables.

---

## 💻 Usage Guide

### 1. Development Server

Start the local development server with Turbopack:

```bash
pnpm dev
# or: npm run dev / yarn dev / bun dev
```

Open [http://localhost:3000](http://localhost:3000) in your browser. Any edits in `app/` will update live via Fast Refresh.

### 2. Building for Production

Create an optimized production build:

```bash
pnpm build
# or: npm run build / yarn build / bun build
```

This compiles your TypeScript, validates MDX routes, and pre-renders static pages using Turbopack.

### 3. Running Production Build

Test your production build locally:

```bash
pnpm start
# or: npm run start / yarn start / bun start
```

### 4. Linting & Formatting with Biome

Check code quality, syntax, and formatting across your project using Biome:

```bash
# Check code for linting and formatting issues
pnpm lint
# or: npm run lint / yarn lint / bun lint

# Automatically fix lint issues and organize imports
pnpm lint:fix
# or: npm run lint:fix / yarn lint:fix / bun lint:fix

# Format files
pnpm format
# or: npm run format / yarn format / bun format
```

---

## 💡 How To Guide

### Creating MDX Pages

This template has native MDX support pre-configured with `@next/mdx`. You can create pages using `.md` or `.mdx` directly in the `app/` directory:

1. Create a new folder with a `page.mdx` file, for example `app/docs/page.mdx`:

```mdx
import { Button } from "@/components/ui";

# Documentation

Welcome to our documentation written in MDX!

- Fast and responsive
- Styled with Tailwind CSS v4 design tokens
- Fully supports React components

<Button variant="default">Interactive Button</Button>
```

2. Navigate to `http://localhost:3000/docs` to see your rendered page.
3. Custom styling for HTML elements (such as `h1`, `pre`, `code`, `blockquote`, and `table`) is centralized in [`mdx-components.tsx`](mdx-components.tsx).

### Using & Adding UI Components

Components are organized into reusable primitives (`components/ui`) and compound app components (`components/app-components`).

#### Importing Components

Use barrel imports from `@/components/ui`:

```tsx
import { Button, Card, CardHeader, CardTitle, CardContent } from "@/components/ui";

export function ProfileCard() {
  return (
    <Card>
      <CardHeader>
        <CardTitle>User Profile</CardTitle>
      </CardHeader>
      <CardContent>
        <Button variant="outline">Edit Profile</Button>
      </CardContent>
    </Card>
  );
}
```

#### Adding More shadcn/ui Primitives

To install additional components using the shadcn CLI:

```bash
# With pnpm
pnpm dlx shadcn@latest add dialog
pnpm dlx shadcn@latest add dropdown-menu
pnpm dlx shadcn@latest add input

# With npm
npx shadcn@latest add dialog
```

After adding a component, remember to re-export it in [`components/ui/index.tsx`](components/ui/index.tsx) for clean barrel imports.

### Dark & Light Mode Theming

Theming is powered by `next-themes` and configured in [`app/layout.tsx`](app/layout.tsx).

- Use the included theme toggle:
  ```tsx
  import { ThemeToggleButton } from "@/components/app-components";

  export function Header() {
    return (
      <header>
        <ThemeToggleButton />
      </header>
    );
  }
  ```
- Or access the active theme in any client component:
  ```tsx
  "use client";

  import { useTheme } from "next-themes";

  export function CustomComponent() {
    const { theme, setTheme } = useTheme();
    // theme is "light", "dark", or "system"
  }
  ```
- Color tokens and theme variables are defined using `oklch` color functions in [`app/globals.css`](app/globals.css).

### Using Hugeicons

Icons are provided by `@hugeicons/react` and `@hugeicons/core-free-icons`:

```tsx
import { HugeiconsIcon } from "@hugeicons/react";
import { SparklesIcon } from "@hugeicons/core-free-icons";

export function FeatureBadge() {
  return (
    <div className="flex items-center gap-2">
      <HugeiconsIcon icon={SparklesIcon} size={18} strokeWidth={2} />
      <span>New Feature</span>
    </div>
  );
}
```

---

## 📜 Available Scripts Summary

| Command | Description |
| :--- | :--- |
| `pnpm dev` | Starts the Next.js development server with Turbopack at `http://localhost:3000` |
| `pnpm build` | Compiles TypeScript and builds the application for production |
| `pnpm start` | Runs the compiled production application |
| `pnpm lint` | Runs Biome check (linter, formatting, imports) across the codebase |
| `pnpm lint:fix` | Runs Biome check and automatically applies safe fixes |
| `pnpm format` | Formats all code files with Biome |

---

## 📄 License

MIT. Feel free to use this template for personal or commercial projects.
