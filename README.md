# MiniGit — Git-Compatible Version Control System in C++20

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg?style=flat-square&logo=cmake)](https://cmake.org/)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.0%2B-721412.svg?style=flat-square&logo=openssl)](https://www.openssl.org/)
[![zlib](https://img.shields.io/badge/zlib-1.2.11%2B-green.svg?style=flat-square)](https://zlib.net/)
[![libcurl](https://img.shields.io/badge/libcurl-7.68%2B-orange.svg?style=flat-square)](https://curl.se/libcurl/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg?style=flat-square)](#building-and-installation)
[![Architecture](https://img.shields.io/badge/Architecture-Content--Addressable%20Storage-success.svg?style=flat-square)](#storage-architecture)
[![Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/total.svg?style=flat-square&logo=github&color=blue)](https://github.com/sagarkrjha/minigit/releases)
[![Lines of Code](https://img.shields.io/endpoint?url=https://ghloc.dev/api/sagarkrjha/minigit/badge&style=flat-square&label=lines%20of%20code&color=informational)](https://github.com/sagarkrjha/minigit)

**MiniGit** is a lightweight, educational, yet architecturally authentic version control system built from scratch in modern **C++20**. Designed as a clean-room behavioral recreation of Git internals, it implements content-addressable object storage, DAG-based commit histories, a two-phase staging index, dynamic programming diff calculation, and full branch management.

> 📖 For practical CLI usage examples, workflows, and recipes, see [USAGE.md](USAGE.md). For architectural and technical specifications, see [FEATURES.md](FEATURES.md).

---

## Download Latest Build

Pre-compiled native standalone binaries are automatically built, verified, and published on every push for Windows, Linux, and macOS. These direct download links always point to the newest successful cross-platform builds:

[![Total Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/total.svg?style=flat-square&logo=github&color=blue)](https://github.com/sagarkrjha/minigit/releases)
[![Latest Release Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/total.svg?style=flat-square&logo=github&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest)

| Platform | Architecture | Binary | Direct Download Link | Downloads |
| :--- | :--- | :--- | :--- | :--- |
| **Windows** | x86_64 | `minigit.exe` | [Download `minigit.exe`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) | [![Windows Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit.exe?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) |
| **Linux** | x86_64 | `minigit-linux` | [Download `minigit-linux`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) | [![Linux Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-linux?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) |
| **macOS** | Apple Silicon (arm64) | `minigit-macos` | [Download `minigit-macos`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) | [![macOS Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-macos?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) |

> ℹ️ These stable direct URLs always point to the newest verified release assets via GitHub's latest release redirect. Download counts are automatically tracked via GitHub Releases.

### Quick Start with Downloaded Binaries

#### Windows (PowerShell / Command Prompt)
Download [`minigit.exe`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) and run directly:
```powershell
# Run directly from PowerShell or Command Prompt
.\minigit.exe init
.\minigit.exe status
```

#### Linux
Download [`minigit-linux`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux), grant execution permissions, and run:
```bash
curl -LO https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux
chmod +x minigit-linux
./minigit-linux init
./minigit-linux status
```

#### macOS
Download [`minigit-macos`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos), grant execution permissions, and run:
```bash
curl -LO https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos
chmod +x minigit-macos
./minigit-macos init
./minigit-macos status
```

---

## Table of Contents

- [Download Latest Build](#download-latest-build)
- [Overview](#overview)
- [Key Features](#key-features)
- [Storage Architecture](#storage-architecture)
- [Command Reference](#command-reference)
- [Building and Installation](#building-and-installation)
  - [Prerequisites](#prerequisites)
  - [Build on Windows (PowerShell / MSVC / Ninja)](#build-on-windows-powershell--msvc--ninja)
  - [Build on Linux / macOS](#build-on-linux--macos)
- [Usage Walkthrough](#usage-walkthrough)
  - [1. Initialize a Repository](#1-initialize-a-repository)
  - [2. Inspect Status](#2-inspect-status)
  - [3. Stage and Commit Changes](#3-stage-and-commit-changes)
  - [4. View History](#4-view-history)
  - [5. Compute Differences](#5-compute-differences)
  - [6. Branching and Switching](#6-branching-and-switching)
  - [7. Detached HEAD and Historic Checkout](#7-detached-head-and-historic-checkout)
  - [8. Tagging Releases](#8-tagging-releases)
  - [9. History Rewriting with Reset and Revert](#9-history-rewriting-with-reset-and-revert)
  - [10. Branch Merging and Conflict Resolution](#10-branch-merging-and-conflict-resolution)
  - [11. Shelving Work with Stash](#11-shelving-work-with-stash)
  - [12. Remote Repositories and Synchronization](#12-remote-repositories-and-synchronization)
  - [13. Low-Level Plumbing Commands](#13-low-level-plumbing-commands)
  - [14. Linear Rebase and Cherry-Pick](#14-linear-rebase-and-cherry-pick)
  - [15. Working Tree Hygiene with Clean](#15-working-tree-hygiene-with-clean)
  - [16. Multiple Linked Worktrees](#16-multiple-linked-worktrees)
  - [17. Nested Submodules](#17-nested-submodules)
  - [18. Binary Search Debugging with Bisect](#18-binary-search-debugging-with-bisect)
  - [19. Packfile Maintenance and Verification](#19-packfile-maintenance-and-verification)
  - [20. Stage and Tree Object Inspection](#20-stage-and-tree-object-inspection)
- [Internal Repository Layout](#internal-repository-layout)
- [Codebase Structure](#codebase-structure)
- [MiniGit vs Standard Git](#minigit-vs-standard-git)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

Canonical Git is often perceived as complex due to decades of accumulated C code, packfile optimizations, and protocol extensions. **MiniGit** strips away legacy baggage while retaining Git's foundational Computer Science elegance:

1. **Content-Addressable Storage (CAS):** Everything is stored as an immutable object addressed by its cryptographic hash (SHA-256 via OpenSSL).
2. **Directed Acyclic Graph (DAG):** Commits form an immutable history graph linked through parent hashes.
3. **Index (Staging Area):** An explicit cache decouples working tree edits from commit creation.
4. **Unified Diff Engine:** Computes line-level edit sequences using Longest Common Subsequence (LCS) dynamic programming.
5. **Branching & HEAD Pointers:** Lightweight pointer-based branches with symbolic and detached reference support.

---

## Key Features

- **Repository Lifecycle:** Create and reinitialize `.minigit` repositories with automatic directory tree and reference initialization.
- **Two-Phase Staging:** Stage granular file changes via `minigit add` and inspect the staged index before committing.
- **Atomic Commits:** Capture tree snapshots, author metadata, Unix timestamps, and commit parentage with `minigit commit`.
- **Revision History:** Linear and ancestor commit history traversal with `minigit log`.
- **Three-Tree Status Inspection:** Real-time state classification (staged, modified, deleted, untracked) between Working Tree, Index, and HEAD via `minigit status`.
- **LCS Diff Engine:** Standard unified diff (`---` / `+++` / `@@ -x,y +x,y @@`) for both unstaged changes and staged changes (`--cached` / `--staged`).
- **Branch Management:** List, create, and safely delete branches with `minigit branch`.
- **Safe Branch Switching:** Dedicated `minigit switch` (including `-c` creation flag) and full working tree restoration via `minigit checkout`.
- **Plumbing Utilities:** Direct object hashing (`minigit hash-object -w`), tree generation (`minigit write-tree`), and object inspection (`minigit cat-file -t/-s/-p`) for scriptability.
- **Ignore Rules:** `.minigitignore` file with glob pattern matching (wildcards, directory patterns, negation with `!`) to exclude files from `status` and `add`.
- **Tagging:** Lightweight and annotated tags via `minigit tag`; annotated tags stored as first-class objects in the object database.
- **History Rewriting & Undo:** Flexible history modification via `minigit reset` (`--soft`, `--mixed`, `--hard`) and non-destructive, history-safe commit inversion via `minigit revert`.
- **Cherry-Pick & Linear Rebase:** Selective changeset transplantation via `minigit cherry-pick` and sequential linear history replay via `minigit rebase` (`--onto`, `--continue`, `--abort`, `--skip`).
- **Three-Way Merge Engine:** Lowest Common Ancestor (LCA) merge-base computation via DAG traversal, fast-forward detection, line-level three-way merging, conflict marker insertion (`<<<<<<<`, `=======`, `>>>>>>>`), and multi-parent merge commits via `minigit merge`.
- **Stash Management:** Temporarily shelve uncommitted working-tree and staging changes with `minigit stash` (`push`, `list`, `pop`, `drop`, `show`).
- **Object Compression:** Deflate compression with transparent backward compatibility via `zlib` for all loose objects in CAS storage.
- **Remotes & Network Synchronization:** Full local and Smart HTTP/HTTPS network remote synchronization workflow (`minigit clone`, `minigit remote`, `minigit fetch`, `minigit push`, `minigit pull`) implementing Git Smart HTTP Transfer Protocol v1 with pkt-line packet framing, transfer negotiation, and packfile streaming.
- **Linked Worktrees:** Check out and work on multiple branches simultaneously using isolated linked working directories (`minigit worktree`), sharing the central CAS object database and reference namespace while preventing branch checkout collisions.
- **Nested Submodules:** Track and coordinate nested repositories using Git-standard mode `160000` gitlink entries, `.minigitmodules` configuration, and full porcelain commands (`minigit submodule` add, status, init, update, deinit, summary, foreach, sync).
- **Binary Search Debugging:** Pinpoint regression-introducing commits across linear and branching DAG histories using `minigit bisect` (`start`, `bad`/`new`, `good`/`old`, `skip`, `reset`, `terms`, `log`, `replay`, and automated `run`).
- **Defensive Engineering:** Path traversal protection (`resolve_safe_repo_path`), internal directory protection (`.minigit`/`.git`), automatic Windows CRLF line-ending normalization, and directory tree discovery.
- **Self-Update & Update Notifications:** Automated version discovery via GitHub Releases API, SemVer precedence comparison, safe executable in-place self-replacement, and non-intrusive CLI terminal update notification banners via `minigit update` and `minigit update --check`.
- **Version Reporting:** Command-line version inspection via `minigit version`, `minigit --version`, or `minigit -v`.

---

## Storage Architecture

MiniGit models your project using four primary object types stored under `.minigit/objects/`:

```text
┌────────────────────────────────────────────────────────┐
│                      COMMIT OBJECT                     │
│  tree: e3b0c44298fc...                                 │
│  parent: 8f4b2a1c90...                                 │
│  author: MiniGit User <user@minigit> 1773322800        │
│  message: Add feature                                  │
└───────────────────────────┬────────────────────────────┘
                            │ points to
                            ▼
┌────────────────────────────────────────────────────────┐
│                       TREE OBJECT                      │
│  100644 src/main.cpp 4a5e1e5823...                     │
│  100644 CMakeLists.txt 9b2d8f1430...                   │
│  160000 libs/submod 5c2d8f1430...                      │
└───────────────┬────────────────────────┬───────────────┘
                │ points to              │ points to
                ▼                        ▼
      ┌──────────────────┐      ┌──────────────────┐
      │   BLOB OBJECT    │      │ ANNOTATED TAG    │
      │ (File Content)   │      │ (object, tag, msg)│
      └──────────────────┘      └──────────────────┘
```

### The Three States

MiniGit coordinates file states across three distinct layers:

```text
Working Directory        Staging Area (Index)       Object Database (Commits)
┌─────────────────┐       ┌─────────────────┐       ┌───────────────────────┐
│                 │  add  │                 │ commit│                       │
│  Current Files  ├──────►│ .minigit/index  ├──────►│  .minigit/objects/    │
│  on Filesystem  │       │ (path -> SHA)   │       │  (Immutable DAG)      │
│                 │◄──────┴─────────────────┴───────┤                       │
└─────────────────┘             checkout            └───────────────────────┘
```

---

## Command Reference

| Command | Category | Description |
| :--- | :--- | :--- |
| `minigit version` \| `--version` \| `-v` | Porcelain | Prints the compiled MiniGit executable version string. |
| `minigit init` | Porcelain | Initializes a new repository or reinitializes an existing one. |
| `minigit status` | Porcelain | Shows working tree, staging area, and untracked file status. |
| `minigit add (<file>\|<dir>\|.)...` | Porcelain | Stages one or more files, directories, or the entire working tree (`.`) into the index. |
| `minigit commit -m <msg> [--author <a>]` | Porcelain | Records staged changes into a new commit object and advances HEAD. |
| `minigit log` | Porcelain | Displays commit logs following parent commit hashes from HEAD. |
| `minigit show [--stat\|--name-only] [<object>]` | Porcelain | Inspects commit metadata with parent diff, annotated tags, trees, or blobs. |
| `minigit diff [--cached\|--staged] [<path>...]` | Porcelain | Displays line-level unified diffs (unstaged or staged). |
| `minigit clean [-f\|--force] [-n\|--dry-run] [-d] [-x] [<path>...]` | Porcelain | Removes untracked files and directories from the working tree. |
| `minigit branch [-d <name> \| <name>]` | Porcelain | Lists, creates, or deletes branches. |
| `minigit switch [-c] <branch>` | Porcelain | Switches to a branch, optionally creating it first with `-c`. |
| `minigit checkout <branch-or-sha>` | Porcelain | Checks out a branch or specific commit, restoring working files. |
| `minigit worktree [add\|list\|remove\|prune\|lock\|unlock\|move]` | Porcelain | Manages multiple linked working directories attached to single repository. |
| `minigit submodule [add\|status\|init\|update\|deinit\|summary\|foreach\|sync]` | Porcelain | Manages nested repositories, `.minigitmodules`, and mode 160000 gitlinks. |
| `minigit bisect [help\|start\|bad\|good\|new\|old\|skip\|reset\|terms\|log\|replay\|run]` | Porcelain | Pinpoints regression-introducing commits using DAG-aware binary search. |
| `minigit tag [name] [-a -m msg] [-d]` | Porcelain | Lists, creates (lightweight or annotated), or deletes tags. |
| `minigit reset [--soft\|--mixed\|--hard] <sha>` | Porcelain | Rolls back HEAD (and optionally index/working tree) to a target commit. |
| `minigit merge <branch> [--author <a>]` | Porcelain | Performs a three-way merge or fast-forward of a branch into HEAD. |
| `minigit revert <commit> [--author <a>]` | Porcelain | Creates a new commit that inverts the changes of a target commit. |
| `minigit cherry-pick [-n\|--no-commit] [--author <a>] [-m <p>] <c>` | Porcelain | Transplants changes from a commit onto current branch. |
| `minigit rebase [-i] [--onto <nb>] <up> \| --continue \| --abort \| --skip` | Porcelain | Replays commits linearly onto upstream base. |
| `minigit stash [push\|list\|pop\|drop\|show]` | Porcelain | Shelves uncommitted changes or restores saved working-tree state. |
| `minigit remote [add\|remove\|-v]` | Porcelain | Manages tracked remote repositories in `.minigit/config`. |
| `minigit clone <repository> [<directory>]` | Porcelain | Clones a repository, sets up `origin` tracking, and checks out HEAD. |
| `minigit fetch [<remote>]` | Porcelain | Downloads objects and remote-tracking refs without altering local branches. |
| `minigit push [<remote> [<branch>]]` | Porcelain | Pushes local branch commits and objects to a remote with fast-forward safety checks. |
| `minigit pull [<remote> [<branch>]]` | Porcelain | Fetches and fast-forwards the active branch to match the remote. |
| `minigit repack [-a] [-d] [-w <n>]` | Porcelain | Consolidates loose objects into binary packfiles with delta compression. |
| `minigit verify-pack [-v\|--verbose] <pack>...` | Plumbing | Validates cryptographic integrity, CRC-32 checksums, and delta chains. |
| `minigit hash-object [-w] <file>` | Plumbing | Computes SHA-256 for a file; optionally persists as a blob. |
| `minigit write-tree` | Plumbing | Serializes current index state into a tree object and prints its SHA. |
| `minigit cat-file (-t\|-s\|-p) <sha>` | Plumbing | Inspects a stored object: prints its type (`-t`), size (`-s`), or pretty-prints its content (`-p`). |
| `minigit ls-files [-s\|-c\|-d\|-m\|-o] [<path>...]` | Plumbing | Inspects staged files, cached status, modifications, deletions, and untracked entries. |
| `minigit ls-tree [-d] [-r] [-t] [--name-only\|--object-only] <tree-ish> [<path>...]` | Plumbing | Traverses and inspects hierarchical tree CAS objects. |

---

## Building and Installation

### Prerequisites

- **C++ Compiler:** Supporting C++20 standard:
  - GCC 11+
  - Clang 13+
  - MSVC 2019 / 2022 (Visual Studio 16.10+)
- **Build System:** CMake 3.20 or newer
- **Cryptographic Library:** OpenSSL 3.0+ (`OpenSSL::Crypto` with SHA-256 support)
- **Compression Library:** zlib (`ZLIB::ZLIB` deflate/inflate compression)
- **Network Client Library:** libcurl (`CURL::libcurl` Smart HTTP client)

---

### Build on Windows (PowerShell / MSVC / Ninja)

#### 1. Clone repository
```powershell
git clone https://github.com/sagarkrjha/minigit.git
cd minigit
```

#### 2. Configure with CMake
Install dependencies via vcpkg:
```powershell
vcpkg install openssl zlib curl:x64-windows
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```
*(Or omit `-G "Ninja"` to use the default Visual Studio generator)*

#### 3. Compile
```powershell
cmake --build build --config Release
```

The executable will be located at:
```powershell
.\build\minigit.exe
```

---

### Build on Linux / macOS

#### 1. Install dependencies
```bash
# Ubuntu / Debian
sudo apt-get update && sudo apt-get install -y build-essential cmake libssl-dev zlib1g-dev libcurl4-openssl-dev

# Fedora
sudo dnf install -y gcc-c++ cmake openssl-devel zlib-devel libcurl-devel

# macOS (Homebrew)
brew install cmake openssl zlib curl
```

#### 2. Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

The binary will be located at `./build/minigit`.

---

## Usage Walkthrough

### 1. Initialize a Repository

Initialize a new empty repository in the current working directory:

```bash
minigit init
```
*Output:*
```text
Initialized empty mini_git repository in C:/path/to/project/.minigit/
```

Running `minigit init` again safely detects and reports the existing repository:
```text
Reinitialized existing mini_git repository in C:/path/to/project/.minigit/
```

---

### 2. Inspect Status

Create sample files and check repository status:

```bash
echo "Hello, MiniGit!" > hello.txt
echo "config=enabled" > app.conf
minigit status
```
*Output:*
```text
On branch main

Untracked files:
	app.conf
	hello.txt
```

---

### 3. Stage and Commit Changes

Stage files to the index (or stage everything using `minigit add .`):

```bash
# Stage specific files or use `minigit add .` to stage all changes
minigit add hello.txt app.conf
minigit status
```
*Output:*
```text
On branch main

Changes to be committed:
	new file:   app.conf
	new file:   hello.txt
```

Create a commit with a custom message and author:

```bash
minigit commit -m "Initial commit with configuration and hello" --author "Alice <alice@example.com>"
```
*Output:*
```text
[(root-commit) 5b2f8a1] Initial commit with configuration and hello
```

---

### 4. View History

View commit logs:

```bash
minigit log
```
*Output:*
```text
commit 5b2f8a18342dc2145b20756e4c7ba987e9e6a0d0d4638708c3525287f3942007
Author: Alice <alice@example.com>
Date:   1773322800

    Initial commit with configuration and hello
```

---

### 5. Compute Differences

#### Inspect Unstaged Modifications
Modify a tracked file without staging:

```bash
echo "Hello, World and MiniGit!" > hello.txt
minigit diff
```
*Output:*
```diff
diff --minigit a/hello.txt b/hello.txt
--- a/hello.txt
+++ b/hello.txt
@@ -1,1 +1,1 @@
-Hello, MiniGit!
+Hello, World and MiniGit!
```

#### Inspect Staged Changes
Stage the change and inspect differences relative to `HEAD`:

```bash
minigit add hello.txt
minigit diff --cached
```

---

### 6. Branching and Switching

#### List Branches
```bash
minigit branch
```
*Output:*
```text
* main
```

#### Create and Switch to a New Branch
```bash
minigit switch -c feature/login
```
*Output:*
```text
Created branch 'feature/login' at 5b2f8a1
Switched to branch 'feature/login'
```

#### Create a Commit on the Branch
```bash
echo "auth_token=secret" >> app.conf
minigit add app.conf
minigit commit -m "Add auth configuration"
```

#### Switch Back to Main
```bash
minigit switch main
```
*Output:*
```text
Switched to branch 'main'
```
*The working tree is automatically updated to the state of `main`!*

#### Delete a Branch
```bash
minigit branch -d feature/login
```

---

### 7. Detached HEAD and Historic Checkout

Time-travel to any commit SHA or branch directly:

```bash
minigit checkout 5b2f8a18342dc2145b20756e4c7ba987e9e6a0d0d4638708c3525287f3942007
```
*Output:*
```text
HEAD is now at 5b2f8a1 Initial commit with configuration and hello
```

Inspect status while in detached state:
```bash
minigit status
```
*Output:*
```text
On branch HEAD (detached)

nothing to commit, working tree clean
```

Return to a branch:
```bash
minigit switch main
```

---

### 8. Tagging Releases

Create lightweight or annotated tags for milestones:

```bash
# Lightweight tag pointing to HEAD commit
minigit tag v1.0.0

# Annotated tag with full metadata and message
minigit tag -a v1.0.1 -m "Release version 1.0.1"

# List all tags
minigit tag

# Delete a tag
minigit tag -d v1.0.0
```

---

### 9. History Rewriting with Reset and Revert

#### Rollback History with `reset`
Undo or adjust commits with three levels of depth:

```bash
# Soft reset: move branch pointer back; staged changes preserved in index
minigit reset --soft <commit-sha>

# Mixed reset (default): move pointer and reset staging area; working files preserved
minigit reset <commit-sha>

# Hard reset: move pointer, reset staging area, and revert working files
minigit reset --hard <commit-sha>
```

#### Safe History Undo with `revert`
Invert the changes of an existing commit by creating a new commit on top of HEAD (safe for public/shared branches):

```bash
minigit revert <commit-sha>
```
*Output:*
```text
[f00c6db] Revert "add line4 and change line1"
```

If subsequent changes conflict with the revert, conflict markers are inserted into the files for manual resolution.

---

### 10. Branch Merging and Conflict Resolution

Merge another branch into your current branch using `minigit merge`:

```bash
# Clean merge or fast-forward:
minigit merge feature/parser
```
*Output:*
```text
Merge made by the 'recursive' strategy.
[5d24932] Merge branch 'feature/parser' into main
```

If conflicting changes exist in the same file region:
```bash
minigit merge feature/ui
```
*Output:*
```text
CONFLICT (content): Merge conflict in app.cpp

Automatic merge failed; fix conflicts and then commit the result.
```

The conflicted files are written with Git-standard conflict markers:
```text
<<<<<<< main
cout << "Version 2.0" << endl;
=======
cout << "Version 2.0-beta" << endl;
>>>>>>> feature/ui
```

Resolve conflicts manually, stage the resolved files with `minigit add`, and finalize with `minigit commit`.

---

### 11. Shelving Work with Stash

Temporarily save dirty working-tree and index modifications without committing:

```bash
# Shelve local changes and restore clean HEAD state
minigit stash

# Inspect saved stashes
minigit stash list
# stash@{0}: WIP on main: 5d24932 Merge branch 'feature/parser' into main

# Inspect files stored inside a stash
minigit stash show stash@{0}

# Restore changes and drop the stash entry
minigit stash pop
```

---

### 12. Remote Repositories and Synchronization

Synchronize commit history and objects between local repositories:

#### 1. Clone a Repository
```bash
# Clone an upstream repository into a new directory
minigit clone /path/to/upstream downstream_repo
cd downstream_repo
```

#### 2. Manage Remotes
```bash
# Inspect configured remotes and their URLs
minigit remote -v
# origin	/path/to/upstream (fetch)
# origin	/path/to/upstream (push)

# Add a new secondary remote
minigit remote add mirror /path/to/backup
```

#### 3. Push and Pull Changes
```bash
# Push newly committed local branch changes to upstream
minigit push origin main

# Fetch changes from upstream without touching working tree
minigit fetch origin

# Fetch and fast-forward active branch with remote updates
minigit pull origin main
```

---

### 13. Low-Level Plumbing Commands

Inspect how MiniGit serializes objects under the hood:

```bash
# Hash a file and print its SHA-256 without writing
minigit hash-object hello.txt

# Hash a file and write it to .minigit/objects
minigit hash-object -w hello.txt

# Snapshot the current staging area into a tree object directly
minigit write-tree
```

#### Inspect stored objects with `cat-file`

Use `minigit cat-file` to read any object from the object store by its SHA-256:

```bash
SHA=$(minigit hash-object -w hello.txt)

# Print the object type
minigit cat-file -t $SHA
# blob

# Print the body size (bytes)
minigit cat-file -s $SHA
# 16

# Pretty-print the object content
minigit cat-file -p $SHA
# Hello, MiniGit!
```

Works on all four object types:

| Flag | blob | tree | commit | tag |
| :--- | :--- | :--- | :--- | :--- |
| `-t` | `blob` | `tree` | `commit` | `tag` |
| `-s` | byte count of file content | byte count of entry list | byte count of commit body | byte count of tag payload |
| `-p` | raw file bytes | `<mode> <type> <sha>    <name>` per entry | formatted headers + blank line + message | tag metadata + blank line + message |

---

### 14. Linear Rebase and Cherry-Pick

#### Cherry-Pick Specific Commits
Transplant an individual commit onto the active branch:
```bash
# Cherry-pick a commit by SHA or branch name
minigit cherry-pick feature/analytics

# Apply changes to staging without creating a commit
minigit cherry-pick -n hotfix/db-patch
```

#### Replay Linear History with Rebase
Rebase current branch commits onto an upstream base:
```bash
# Rebase feature branch onto main
minigit switch feature/payment
minigit rebase main

# Rebase onto a specific base
minigit rebase --onto main feature/legacy-auth

# Resolve conflicts, stage files, and continue or abort
minigit add src/payment.cpp
minigit rebase --continue
# or: minigit rebase --abort | minigit rebase --skip
```

---

### 15. Working Tree Hygiene with Clean

Remove untracked artifacts from the working tree safely (enforces `-f` or `-n`):
```bash
# Preview files that would be deleted (dry-run)
minigit clean -n

# Force removal of untracked files and untracked directories (-d)
minigit clean -fd

# Clean including ignored files (-x)
minigit clean -fdx
```

---

### 16. Multiple Linked Worktrees

Check out and work on multiple branches simultaneously using isolated working directories:
```bash
# Add a new linked worktree checking out a new branch
minigit worktree add ../feature-auth -b feature/oauth2

# List all active linked worktrees
minigit worktree list

# Lock worktree to prevent pruning
minigit worktree lock --reason "Offline development" feature-auth

# Unlock and remove worktree
minigit worktree unlock feature-auth
minigit worktree remove ../feature-auth

# Prune administrative metadata for deleted worktrees
minigit worktree prune -v
```

---

### 17. Nested Submodules

Coordinate nested repositories using Git-standard mode `160000` gitlink entries:
```bash
# Add an external repository as a submodule
minigit submodule add https://github.com/example/math.git libs/math
minigit commit -m "Add math submodule"

# Check status across submodules
minigit submodule status

# Initialize and update submodules on a fresh clone
minigit submodule update --init --recursive

# Run commands in all submodule working trees
minigit submodule foreach minigit status
```

---

### 18. Binary Search Debugging with Bisect

Pinpoint regression-introducing commits across the commit DAG in $O(\log N)$ steps:
```bash
# 1. Interactive bisection
minigit bisect start
minigit bisect bad HEAD
minigit bisect good v1.0.0
# MiniGit checks out optimal DAG midpoints automatically:
minigit bisect bad      # if test fails
minigit bisect good     # if test passes
minigit bisect reset    # clean up and return to starting branch

# 2. Fully automated bisection runner
minigit bisect start HEAD v1.0.0
minigit bisect run ./scripts/test_regression.sh
```

---

### 19. Packfile Maintenance and Verification

Consolidate thousands of loose CAS objects into binary `.pack` archives with byte-level sliding-window delta compression:
```bash
# Consolidate loose objects and delete unpacked sources (-d)
minigit repack -d -w 10

# Verify packfile SHA-256 and CRC-32 integrity
minigit verify-pack .minigit/objects/pack/*.pack

# Verbose inspection showing object types, unpacked sizes, and delta chains
minigit verify-pack -v .minigit/objects/pack/*.pack
```

---

### 20. Stage and Tree Object Inspection

Directly inspect staging area contents and hierarchical tree objects:
```bash
# Inspect tracked files, staged modes, and blob hashes
minigit ls-files -s

# Show untracked files respecting .minigitignore
minigit ls-files -o

# Traverse tree object hierarchy recursively
minigit ls-tree -r HEAD

# Output only filenames or object hashes
minigit ls-tree -r --name-only HEAD
minigit ls-tree -r --object-only HEAD
```

---

## Internal Repository Layout

When `minigit init` is executed, it creates the `.minigit` directory structure:

```text
.minigit/
├── HEAD               # Symbolic ref (ref: refs/heads/main) or commit SHA
├── config             # Repository configuration file (remotes, submodules)
├── index              # Staging area: flat map of path -> SHA-256 entries
├── stash              # Stash commit stack (one SHA per line)
├── BISECT_START       # Active bisection session starting reference
├── BISECT_LOG         # Audit log of bisection steps
├── BISECT_TERMS       # Custom bisection terms (bad/good or custom)
├── rebase-apply/      # Transient linear rebase transplantation state
├── worktrees/         # Linked working tree administrative metadata
│   └── <id>/          # Per-worktree gitdir, commondir, HEAD, index
├── modules/           # Nested submodule CAS and ref storage
│   └── <name>/        # Submodule object database and reference namespace
├── objects/           # Content-addressable zlib-compressed object store
│   ├── pack/          # Packfile (.pack) and index (.idx) binary archives
│   ├── 5b/
│   │   └── 2f8a18342dc2145b...   # Blob / Tree / Commit / Tag object payloads
│   └── ...
└── refs/
    ├── heads/         # Branch pointers (e.g., refs/heads/main)
    ├── remotes/       # Remote-tracking branches (e.g., refs/remotes/origin/main)
    ├── tags/          # Tag pointers (lightweight and annotated)
    └── bisect/        # Active bisection boundary references (bad, good-*, skip-*)
```

### On-Disk Object Envelope

All objects in `.minigit/objects/` are formatted with an envelope header:

```text
<type> <byte_size>\0<content>
```

- **Blob:** `blob <size>\0<file-bytes>`
- **Tree:** `tree <size>\0<mode> <filename> <sha256>\n...`
- **Commit:** `commit <size>\0tree <sha>\nparent <sha>\nauthor <author> <timestamp>\ncommitter <author> <timestamp>\n\n<message>`
- **Tag:** `tag <size>\0object <sha>\ntype commit\ntag <name>\ntagger <author> <timestamp>\n\n<message>`

Objects are sharded using the first 2 characters of their 64-character hex SHA-256 hash as the subdirectory name and the remaining 62 characters as the filename.

---

## Codebase Structure

```text
minigit/
├── CMakeLists.txt              # Root CMake build configuration
├── README.md                   # Project overview and user guide
├── USAGE.md                    # Detailed CLI usage recipes and cheat sheet
├── FEATURES.md                 # In-depth architectural and feature specification
├── src/
│   ├── CMakeLists.txt          # Modular CMake targets & executable linking
│   ├── main.cpp                # Minimal executable entry point
│   ├── cli/                    # CLI Dispatcher & Command Router
│   │   ├── CMakeLists.txt
│   │   ├── dispatcher.h
│   │   └── dispatcher.cpp
│   ├── core/                   # Shared Infrastructure & Utilities
│   │   ├── CMakeLists.txt
│   │   ├── file.{h,cpp}        # File reading and binary I/O helpers
│   │   ├── path_safety.{h,cpp} # Path traversal validation & repo boundary safety
│   │   ├── sha256.{h,cpp}      # OpenSSL SHA-256 cryptographic hashing
│   │   └── zlib_compress.{h,cpp}# zlib deflate / inflate compression
│   ├── repository/             # Repository Lifecycle & Initialization
│   │   ├── CMakeLists.txt
│   │   ├── repository.{h,cpp}  # Directory discovery and .minigit structure
│   │   └── init.{h,cpp}        # init command
│   ├── storage/                # Content-Addressable Storage (CAS) & Plumbing
│   │   ├── CMakeLists.txt
│   │   ├── blob.{h,cpp}        # Blob object representation
│   │   ├── tree.{h,cpp}        # Tree object representation
│   │   ├── commit.{h,cpp}      # Commit domain object representation
│   │   ├── object_database.{h,cpp} # Sharded on-disk CAS store with zlib compression
│   │   ├── object_parser.{h,cpp}   # Raw byte parsing into domain structs
│   │   ├── pack.{h,cpp}        # Packfile v2 & idx v2 reader/writer & delta compression
│   │   ├── repack.{h,cpp}      # repack command: loose object compaction
│   │   ├── hash_object.{h,cpp} # hash-object plumbing command
│   │   ├── cat_file.{h,cpp}    # cat-file plumbing command
│   │   └── ls_tree.{h,cpp}     # ls-tree plumbing command
│   ├── staging/                # Staging Area, Index & Working Tree State
│   │   ├── CMakeLists.txt
│   │   ├── index.{h,cpp}       # In-memory and on-disk index manager
│   │   ├── ignore.{h,cpp}      # .minigitignore parser and glob matcher
│   │   ├── add.{h,cpp}         # add command: recursive staging & deletion sync
│   │   ├── status.{h,cpp}      # status command: multi-tree delta calculation
│   │   ├── clean.{h,cpp}       # clean command: untracked file hygiene
│   │   ├── reset.{h,cpp}       # reset command: soft, mixed, and hard resets
│   │   ├── ls_files.{h,cpp}    # ls-files plumbing command
│   │   └── write_tree.{h,cpp}  # write-tree plumbing command
│   ├── history/                # Commit History & Log
│   │   ├── CMakeLists.txt
│   │   ├── commit.{h,cpp}      # commit command: snapshot index and update branch
│   │   ├── log.{h,cpp}         # log command: commit graph traversal
│   │   └── show.{h,cpp}        # show command: commit, tag, tree, blob inspection
│   ├── branching/              # Branches, References & Switching
│   │   ├── CMakeLists.txt
│   │   ├── branch.{h,cpp}      # branch command: list, create, delete branches
│   │   ├── checkout.{h,cpp}    # checkout command: restore tree & detached HEAD
│   │   ├── switch_branch.{h,cpp}# switch command: branch switching
│   │   └── tag.{h,cpp}         # tag command: lightweight & annotated tags
│   ├── diff/                   # Diff Engine & Command
│   │   ├── CMakeLists.txt
│   │   ├── diff_engine.{h,cpp} # LCS dynamic programming algorithm & unified diff
│   │   └── diff.{h,cpp}        # diff CLI command
│   ├── merge/                  # Merge, Rebase & Cherry-Pick Engine
│   │   ├── CMakeLists.txt
│   │   ├── merge_engine.{h,cpp}# 3-way line merge & LCA DAG traversal
│   │   ├── merge.{h,cpp}       # merge command
│   │   ├── revert.{h,cpp}      # revert command
│   │   ├── cherry_pick.{h,cpp} # cherry-pick command: selective transplantation
│   │   └── rebase.{h,cpp}      # rebase command: linear history replay
│   ├── stash/                  # Working-State Shelving
│   │   ├── CMakeLists.txt
│   │   └── stash.{h,cpp}       # stash command: push, list, pop, drop, show
│   ├── worktree/               # Multiple Linked Working Trees
│   │   ├── CMakeLists.txt
│   │   └── worktree.{h,cpp}    # worktree add, list, remove, prune, lock, unlock, move
│   ├── submodule/              # Nested Repositories & Gitlink Management
│   │   ├── CMakeLists.txt
│   │   ├── submodule_config.{h,cpp} # .minigitmodules & config INI manager
│   │   └── submodule.{h,cpp}   # submodule add, status, init, update, deinit, summary, foreach, sync
│   ├── bisect/                 # Binary Search Debugging Subsystem
│   │   ├── CMakeLists.txt
│   │   └── bisect.{h,cpp}      # bisect start, bad, good, skip, reset, terms, log, replay, run
│   └── remotes/                # Remote Synchronization & Transport Protocol
│       ├── CMakeLists.txt
│       ├── pkt_line.{h,cpp}    # 4-hex pkt-line framing, flush/delim, ref parsing
│       ├── http_client.{h,cpp} # libcurl RAII client wrapper (GET/POST, SSL options)
│       ├── pack_unpack.{h,cpp} # Packfile streaming, sideband extraction & CAS unpack
│       ├── smart_http.{h,cpp}  # Smart HTTP discovery, clone_http, fetch_http, push_http
│       ├── config.{h,cpp}      # .minigit/config INI parser and remote manager
│       ├── transfer.{h,cpp}    # BFS missing-object DAG traversal & ancestry checker
│       ├── remote.{h,cpp}      # remote command: list, add, remove remotes
│       ├── clone.{h,cpp}       # clone command (local & Smart HTTP)
│       ├── fetch.{h,cpp}       # fetch command (local & Smart HTTP)
│       ├── push.{h,cpp}        # push command (local & Smart HTTP)
│       └── pull.{h,cpp}        # pull command (local & Smart HTTP)
└── tests/                      # Automated Regression & Verification Harness
    ├── CMakeLists.txt
    ├── test_framework.h        # Self-registering test harness macro suite
    ├── benchmark_metrics.cpp   # Empirical performance benchmarks
    ├── test_main.cpp           # Test executable runner entry point
    ├── test_core.cpp           # Cryptographic hashing, compression, path safety
    ├── test_repository.cpp     # Repository lifecycle & discovery
    ├── test_storage.cpp        # CAS blobs, trees, commits, ODB
    ├── test_staging.cpp        # Index roundtripping & ignore patterns
    ├── test_diff.cpp           # LCS dynamic programming diff engine
    ├── test_show.cpp           # Object inspection & diffstat formatting
    ├── test_clean.cpp          # Working tree cleanup safeguards
    ├── test_ls.cpp             # ls-files and ls-tree inspection
    ├── test_cherry_pick.cpp    # Commit transplantation & conflict detection
    ├── test_rebase.cpp         # Linear rebase replay, onto, abort, skip
    ├── test_pack.cpp           # Packfile v2, idx v2, delta compression roundtrips
    ├── test_worktree.cpp       # Linked worktree isolation & exclusivity
    ├── test_submodule.cpp      # Submodule gitlinks & .minigitmodules
    ├── test_bisect.cpp         # DAG midpoint binary search & automated run
    └── test_smart_http.cpp     # Smart HTTP pkt-line protocol & transfers
```

---

## MiniGit vs Standard Git

While MiniGit provides high behavioral fidelity, shared low-level concepts, and wire-level interoperability with standard Git over Smart HTTP and local filesystems, it makes deliberate architectural choices prioritizing educational clarity, modern C++20 memory safety, cryptographic robustness (pure SHA-256), and deterministic single-pass performance.

### Architectural Comparison Matrix

| Architectural Dimension | MiniGit (v1.8.2) | Canonical / Standard Git | Codebase Reference |
| :--- | :--- | :--- | :--- |
| **Implementation Language** | Modern C++20 (`std::filesystem`, RAII, OOP) | C99, POSIX shell scripts, Perl | Full codebase |
| **Repository Root Directory** | `.minigit/` (isolated metadata environment) | `.git/` | [`src/repository/`](src/repository/) |
| **Cryptographic Hash** | Pure SHA-256 (64 hex characters) via OpenSSL EVP | SHA-1 (40 hex chars default); experimental SHA-256 | [`src/core/sha256.cpp`](src/core/sha256.cpp) |
| **Tree Object Model** | **Flat Tree**: Commit points to one tree storing all relative paths (`<mode> <path> <sha256>\n`) | **Hierarchical Tree DAG**: Tree-of-trees where subdirectories are nested subtree objects (`040000 tree`) | [`src/storage/tree.cpp`](src/storage/tree.cpp) |
| **Index / Staging Area** | Plaintext `<path> <sha256>` line entries; single-stage in-memory hash map; no filesystem `stat` cache | Binary `DIRC` format (v2–v4) with 40-byte stat cache (ctime/mtime/ino/dev/size) and 4-stage conflict flags | [`src/staging/index.cpp`](src/staging/index.cpp) |
| **Merge Engine & Conflicts** | 3-way LCA line merge via LCS DP; conflicts inject markers and are directly staged into the flat index | Pluggable strategies (`ort`, `recursive`, `octopus`); conflicts split index into stages 1, 2, and 3 until `git add` | [`src/merge/merge.cpp`](src/merge/merge.cpp) |
| **Packfile & Delta Compression** | Packfile v2 & Index v2 with single-depth (`depth <= 1`) `OBJ_REF_DELTA` for deterministic $O(1)$ unpack reads | Packfile v2 with arbitrary delta chain depth ($\le 50$), `OBJ_OFS_DELTA` relative offsets, MIDX, and bitmaps | [`src/storage/pack.cpp`](src/storage/pack.cpp) |
| **Working Tree Checkout** | `minigit checkout <target>` restores full tree snapshots (branch/commit); does not checkout individual paths | `git checkout` switches branches, detaches HEAD, restores individual pathspecs (`-- <path>`), and checks out hunks (`-p`) | [`src/branching/checkout.cpp`](src/branching/checkout.cpp) |
| **Diff Engine & Formatting** | Eugene Myers' $O(ND)$ greedy difference algorithm ([`myers_diff`](src/diff/diff_engine.cpp)); unified diff header formatted as `diff --minigit a/... b/...` | Eugene Myers' greedy diff algorithm ($O(ND)$) / patience diff; unified diff header formatted as `diff --git a/... b/...` | [`src/diff/diff_engine.cpp`](src/diff/diff_engine.cpp) |
| **Network & Wire Protocols** | Git Smart HTTP v1 client (`git-upload-pack`/`git-receive-pack` via libcurl pkt-line) and local filesystem | Full multi-protocol suite: Smart HTTP v1 & v2, SSH (`git@`), Git daemon (`git://`), dumb HTTP, and bundles | [`src/remotes/smart_http.cpp`](src/remotes/smart_http.cpp) |
| **Configuration Files** | `.minigitignore`, `.minigitmodules`, and `.minigit/config` | Hierarchical config (`system`, `global`, `local`, `worktree`), nested `.gitignore`, `.gitmodules`, `.git/config` | [`src/remotes/config.cpp`](src/remotes/config.cpp) |
| **Commit & Tag Metadata** | Author and committer share single Unix epoch timestamp (no timezone offset); annotated tags are first-class tag objects | Independent author & committer identities, timestamps, and timezone offsets (`+HHMM`/`-HHMM`); GPG/SSH signing | [`src/storage/commit.cpp`](src/storage/commit.cpp) |
| **Linked Worktrees** | Full support (`add`, `list`, `remove`, `prune`, `lock`, `unlock`, `move`) with active branch exclusivity | Full support (`git worktree`) | [`src/worktree/worktree.cpp`](src/worktree/worktree.cpp) |
| **Submodules** | Full support (`add`, `status`, `init`, `update`, `deinit`, `summary`, `foreach`, `sync`) with mode `160000` gitlinks | Full support (`git submodule`) | [`src/submodule/submodule.cpp`](src/submodule/submodule.cpp) |
| **Binary Search Debugging** | Full DAG midpoint bisection (`start`, `bad`, `good`, `skip`, `reset`, `terms`, `log`, `replay`, `run`) | Full support (`git bisect`) | [`src/bisect/bisect.cpp`](src/bisect/bisect.cpp) |
| **Command Dispatching** | Static compile-time $O(1)$ dispatch tables using `std::unordered_map<std::string_view, CommandHandler>` | Built-in command array (`struct cmd_struct`) with prefix abbreviations, external pager, and aliases | [`src/cli/dispatcher.cpp`](src/cli/dispatcher.cpp) |

### Key Architectural Divergences Explained

1. **Flat Tree Model vs. Hierarchical Subtrees:**
   Standard Git represents directory structures as a tree-of-trees DAG, where subdirectories are separate `tree` objects with mode `040000`. MiniGit flattens the tree hierarchy into a single sorted text record list per commit, with entries storing the complete relative path (`100644 src/core/sha256.cpp <sha256>`). This eliminates recursive subtree pointer chasing during tree traversals and diffs while maintaining unambiguous file paths.

2. **Index Structure & Conflict Lifecycle:**
   Canonical Git uses a binary `DIRC` index maintaining 40-byte stat cache entries (device, inode, mode, UID, GID, mtime, file size) to avoid rehashing unchanged files. During merge conflicts, it records up to three distinct stages for conflicting paths (stage 1 = common ancestor, stage 2 = target/ours, stage 3 = incoming/theirs). MiniGit uses a clean plaintext line-oriented index (`<path> <sha256>`) mapped to an in-memory hash table. During conflicts, MiniGit writes standard conflict markers directly into working tree files and immediately hashes and stages the marked content into the flat index, signaling conflict resolution state through file markers and CLI exit codes rather than multi-stage index slots.

3. **Packfile Delta Depth & Traversal Guarantees:**
   Canonical Git constructs deep delta chains (often 10–50 deltas deep) combining both `OBJ_REF_DELTA` and `OBJ_OFS_DELTA` offsets to maximize compression ratios. MiniGit's Packfile v2 engine enforces a strict maximum delta depth of 1 (`OBJ_REF_DELTA` bases are never allowed to be deltas themselves). This bounds object reconstruction time to exactly one delta application step ($O(1)$ unpack complexity), preventing delta chain traversal degradation.

4. **Working Tree Checkout Scope:**
   Canonical Git's `git checkout` is a polymorphic tool that switches branches, detaches HEAD, restores individual pathspecs (`git checkout <commit> -- <path>`), and interactively restores hunks (`git checkout -p`). MiniGit explicitly cleanly decouples branch operations (`minigit switch` / `minigit switch -c`) from full-tree snapshot restoration (`minigit checkout <branch|commit>`). MiniGit's checkout restores the complete tree snapshot and updates HEAD; granular file restoration is managed via clean working tree operations.

5. **Diff Algorithm & Output Headers:**
   Both MiniGit and canonical Git implement Eugene Myers' 1986 $O(ND)$ difference algorithm (*"An $O(ND)$ Difference Algorithm and Its Variations"*). MiniGit's diff engine in [`src/diff/diff_engine.cpp`](src/diff/diff_engine.cpp) implements the greedy diagonal search with $O(D^2)$ space trace slicing, outputting unified diff hunks formatted with `diff --minigit a/<path> b/<path>` headers and automatically stripping Windows CRLF carriage returns. Canonical Git outputs `diff --git a/<path> b/<path>` and provides alternative heuristic drivers (patience/histogram diff).

6. **Transport & Wire Protocol Interoperability:**
   MiniGit is directly wire-compatible with standard Git Smart HTTP servers over HTTP/HTTPS. It implements the Git Smart HTTP v1 transfer protocol (`git-upload-pack` and `git-receive-pack`) via libcurl, parsing 4-hex pkt-line frames, discovering advertised refs, negotiating `want`/`have` sets, and streaming packfiles directly into the local Content-Addressable Storage (CAS). For multi-protocol flexibility, canonical Git also supports SSH, native Git daemon (`git://`), dumb HTTP, and bundle archives.

---

## Roadmap

Planned milestones for future MiniGit development:

- [x] **Phase 1: Ignore Rules:** `.minigitignore` glob pattern matching — excludes files from `status` and `add` with support for wildcards, directory patterns (`build/`), and negation (`!pattern`).
- [x] **Phase 2: Tagging:** Lightweight and annotated tags (`minigit tag`) stored under `refs/tags/`; annotated tags are first-class objects in the object database.
- [x] **Phase 3: Three-Way Merging:** Merge base computation, automatic three-way file merge, and conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`).
- [x] **Phase 4: History Rewriting & Safe Undo:** `minigit reset` (`--soft`, `--mixed`, `--hard`) and `minigit revert` (three-way inverse commit application with conflict detection).
- [x] **Phase 5: Stash & Compression:** `minigit stash` working-state shelving (`push`, `list`, `pop`, `drop`, `show`) and zlib deflate loose object compression.
- [x] **Phase 6: Networking & Remotes:** Local filesystem remotes protocol with `clone`, `remote`, `fetch`, `push`, and `pull`.
- [x] **Phase 7: Packfiles & Delta Compression:** Object database consolidation into binary packfiles (`.pack`), `.idx` fan-out index, and sliding-window byte-level delta compression (`minigit repack`, `minigit verify-pack`).
- [x] **Phase 8: Smart HTTP Remotes:** Remote synchronization over HTTP/HTTPS with bidirectional packet-line (pkt-line) framing, ref discovery, packfile streaming, and transfer negotiation implemented in v1.8.0.
- [x] **Phase 9: Linear Rebase & Cherry-Pick:** Linear history rewriting and replay via `minigit rebase` (`--onto`, `--continue`, `--abort`, `--skip`), and individual commit transplantation (`minigit cherry-pick`).
- [x] **Phase 10: Worktrees & Submodules:** Multiple linked working trees (`minigit worktree`) implemented in v1.5.0; nested repository tracking and gitlinks (`minigit submodule`) implemented in v1.6.0.
- [x] **Phase 11: Binary Search Debugging:** DAG-aware binary search debugging (`minigit bisect`) with automated test execution (`bisect run`), session recording/replay, and customizable terms implemented in v1.7.0.
- [x] **Phase 12: Security Hardening & Performance Profiling:** Path traversal prevention across checkout/clone/pull, hardened network pkt-line and packfile decoders, RAII resource lifecycle guards for libcurl, and dedicated empirical performance benchmarks (`minigit_benchmarks`) implemented in v1.8.1.
- [x] **Phase 13: Lookup & Dispatch Optimization:** Replaced linear sequential conditional checks and command/subcommand parsing with constant-time `std::unordered_map` dispatch tables across worktree, submodule, bisect, show, dispatcher, and pack storage subsystem handlers in v1.8.2.
- [x] **Phase 14: Diagnostic & Trace Logging Subsystem:** Zero-cost internal diagnostic tracing and multi-level logging (`MINIGIT_TRACE`, `--trace`, `--log-level`) implemented in v1.9.0.
- [x] **Phase 15: Self-Update & CLI Update Notification Banner:** Automated in-place self-update (`minigit update`), release discovery, and cached terminal update notification banners implemented in v1.10.0.

---

## License

This project is licensed under the MIT License — see the [LICENCE](LICENCE) file for details.
