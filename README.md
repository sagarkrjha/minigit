# MiniGit — Git-Compatible Version Control System in C++20

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C.svg?style=flat-square&logo=cmake)](https://cmake.org/)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.0%2B-721412.svg?style=flat-square&logo=openssl)](https://www.openssl.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg?style=flat-square)](#building-and-installation)
[![Architecture](https://img.shields.io/badge/Architecture-Content--Addressable%20Storage-success.svg?style=flat-square)](#storage-architecture)

**MiniGit** is a lightweight, educational, yet architecturally authentic version control system built from scratch in modern **C++20**. Designed as a clean-room behavioral recreation of Git internals, it implements content-addressable object storage, DAG-based commit histories, a two-phase staging index, dynamic programming diff calculation, and full branch management.

> 📖 For practical CLI usage examples, workflows, and recipes, see [USAGE.MD](USAGE.MD). For architectural and technical specifications, see [FEATURES.md](FEATURES.md).

---

## Download Latest Build

Pre-compiled native standalone binaries are automatically built, verified, and published on every push for Windows, Linux, and macOS. These direct download links always point to the newest successful cross-platform builds:

| Platform | Architecture | Binary | Direct Download Link |
| :--- | :--- | :--- | :--- |
| **Windows** | x86_64 | `minigit.exe` | [Download `minigit.exe`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit.exe) |
| **Linux** | x86_64 | `minigit-linux` | [Download `minigit-linux`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-linux) |
| **macOS** | Apple Silicon (arm64) | `minigit-macos` | [Download `minigit-macos`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-macos) |

> ℹ️ These stable direct URLs always point to the newest verified release assets.

### Quick Start with Downloaded Binaries

#### Windows (PowerShell / Command Prompt)
Download [`minigit.exe`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit.exe) and run directly:
```powershell
# Run directly from PowerShell or Command Prompt
.\minigit.exe init
.\minigit.exe status
```

#### Linux
Download [`minigit-linux`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-linux), grant execution permissions, and run:
```bash
curl -LO https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-linux
chmod +x minigit-linux
./minigit-linux init
./minigit-linux status
```

#### macOS
Download [`minigit-macos`](https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-macos), grant execution permissions, and run:
```bash
curl -LO https://github.com/sagarkrjha/minigit/releases/download/latest/minigit-macos
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
- **Three-Way Merge Engine:** Lowest Common Ancestor (LCA) merge-base computation via DAG traversal, fast-forward detection, line-level three-way merging, conflict marker insertion (`<<<<<<<`, `=======`, `>>>>>>>`), and multi-parent merge commits via `minigit merge`.
- **Stash Management:** Temporarily shelve uncommitted working-tree and staging changes with `minigit stash` (`push`, `list`, `pop`, `drop`, `show`).
- **Object Compression:** Deflate compression with transparent backward compatibility via `zlib` for all loose objects in CAS storage.
- **Remotes & Synchronization:** Full local remote synchronization workflow: clone repositories with `minigit clone`, manage remotes with `minigit remote`, fetch updates with `minigit fetch`, fast-forward push with `minigit push`, and pull latest changes with `minigit pull`.
- **Defensive Engineering:** Path traversal protection (`..` escape checks), automatic Windows CRLF line-ending normalization, and directory tree discovery.

---

## Storage Architecture

MiniGit models your project using three primary object types stored under `.minigit/objects/`:

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
└───────────────┬────────────────────────┬───────────────┘
                │ points to              │ points to
                ▼                        ▼
      ┌──────────────────┐      ┌──────────────────┐
      │   BLOB OBJECT    │      │   BLOB OBJECT    │
      │ (File Content A) │      │ (File Content B) │
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
| `minigit init` | Porcelain | Initializes a new repository or reinitializes an existing one. |
| `minigit status` | Porcelain | Shows working tree, staging area, and untracked file status. |
| `minigit add (<file>\|<dir>\|.)...` | Porcelain | Stages one or more files, directories, or the entire working tree (`.`) into the index. |
| `minigit commit -m <msg> [--author <a>]` | Porcelain | Records staged changes into a new commit object and advances HEAD. |
| `minigit log` | Porcelain | Displays commit logs following parent commit hashes from HEAD. |
| `minigit diff [--cached] [<path>...]` | Porcelain | Displays line-level unified diffs (unstaged or staged). |
| `minigit branch [name] [-d name]` | Porcelain | Lists, creates, or deletes branches. |
| `minigit switch [-c] <branch>` | Porcelain | Switches to a branch, optionally creating it first with `-c`. |
| `minigit checkout <branch-or-sha>` | Porcelain | Checks out a branch or specific commit, restoring working files. |
| `minigit tag [name] [-a -m msg] [-d]` | Porcelain | Lists, creates (lightweight or annotated), or deletes tags. |
| `minigit reset [--soft\|--mixed\|--hard] <sha>` | Porcelain | Rolls back HEAD (and optionally index/working tree) to a target commit. |
| `minigit merge <branch> [--author <a>]` | Porcelain | Performs a three-way merge or fast-forward of a branch into HEAD. |
| `minigit revert <commit> [--author <a>]` | Porcelain | Creates a new commit that inverts the changes of a target commit. |
| `minigit stash [push\|list\|pop\|drop\|show]` | Porcelain | Shelves uncommitted changes or restores saved working-tree state. |
| `minigit remote [add\|remove\|-v]` | Porcelain | Manages tracked remote repositories in `.minigit/config`. |
| `minigit clone <repository> [<directory>]` | Porcelain | Clones a repository, sets up `origin` tracking, and checks out HEAD. |
| `minigit fetch [<remote>]` | Porcelain | Downloads objects and remote-tracking refs without altering local branches. |
| `minigit push [<remote> [<branch>]]` | Porcelain | Pushes local branch commits and objects to a remote with fast-forward safety checks. |
| `minigit pull [<remote> [<branch>]]` | Porcelain | Fetches and fast-forwards the active branch to match the remote. |
| `minigit hash-object [-w] <file>` | Plumbing | Computes SHA-256 for a file; optionally persists as a blob. |
| `minigit write-tree` | Plumbing | Serializes current index state into a tree object and prints its SHA. |
| `minigit cat-file (-t\|-s\|-p) <sha>` | Plumbing | Inspects a stored object: prints its type (`-t`), size (`-s`), or pretty-prints its content (`-p`). |

---

## Building and Installation

### Prerequisites

- **C++ Compiler:** Supporting C++20 standard:
  - GCC 11+
  - Clang 13+
  - MSVC 2019 / 2022 (Visual Studio 16.10+)
- **Build System:** CMake 3.20 or newer
- **Cryptographic Library:** OpenSSL (`OpenSSL::Crypto` with SHA-256 support)

---

### Build on Windows (PowerShell / MSVC / Ninja)

#### 1. Clone repository
```powershell
git clone https://github.com/your-username/minigit.git
cd minigit
```

#### 2. Configure with CMake
If OpenSSL is installed via vcpkg or system path:
```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
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
sudo apt-get update && sudo apt-get install -y build-essential cmake libssl-dev

# Fedora
sudo dnf install -y gcc-c++ cmake openssl-devel

# macOS (Homebrew)
brew install cmake openssl
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

Works on all three object types:

| Flag | blob | tree | commit |
| :--- | :--- | :--- | :--- |
| `-t` | `blob` | `tree` | `commit` |
| `-s` | byte count of file content | byte count of entry list | byte count of commit body |
| `-p` | raw file bytes | `<mode> <type> <sha>    <name>` per entry | formatted headers + blank line + message |

---

## Internal Repository Layout

When `minigit init` is executed, it creates the `.minigit` directory structure:

```text
.minigit/
├── HEAD               # Symbolic ref (ref: refs/heads/main) or commit SHA
├── config             # Repository configuration file (including remotes)
├── index              # Staging area: flat map of path -> SHA-256 entries
├── stash              # Stash commit stack (one SHA per line)
├── objects/           # Content-addressable zlib-compressed object store
│   ├── 5b/
│   │   └── 2f8a18342dc2145b...   # Blob / Tree / Commit object payloads
│   └── ...
└── refs/
    ├── heads/         # Branch pointers (e.g., refs/heads/main)
    ├── remotes/       # Remote-tracking branches (e.g., refs/remotes/origin/main)
    └── tags/          # Tag pointers
```

### On-Disk Object Envelope

All objects in `.minigit/objects/` are formatted with an envelope header:

```text
<type> <byte_size>\0<content>
```

- **Blob:** `blob <size>\0<file-bytes>`
- **Tree:** `tree <size>\0<mode> <filename> <sha256>\n...`
- **Commit:** `commit <size>\0tree <sha>\nparent <sha>\nauthor <author> <timestamp>\ncommitter <author> <timestamp>\n\n<message>`

Objects are sharded using the first 2 characters of their 64-character hex SHA-256 hash as the subdirectory name and the remaining 62 characters as the filename.

---

## Codebase Structure

```text
minigit/
├── CMakeLists.txt              # Root CMake build configuration
├── README.md                   # Project overview and user guide
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
│   │   ├── hash_object.{h,cpp} # hash-object plumbing command
│   │   └── cat_file.{h,cpp}    # cat-file plumbing command
│   ├── staging/                # Staging Area, Index & Working Tree State
│   │   ├── CMakeLists.txt
│   │   ├── index.{h,cpp}       # In-memory and on-disk index manager
│   │   ├── ignore.{h,cpp}      # .minigitignore parser and glob matcher
│   │   ├── add.{h,cpp}         # add command: recursive staging & deletion sync
│   │   ├── status.{h,cpp}      # status command: multi-tree delta calculation
│   │   ├── reset.{h,cpp}       # reset command: soft, mixed, and hard resets
│   │   └── write_tree.{h,cpp}  # write-tree plumbing command
│   ├── history/                # Commit History & Log
│   │   ├── CMakeLists.txt
│   │   ├── commit.{h,cpp}      # commit command: snapshot index and update branch
│   │   └── log.{h,cpp}         # log command: commit graph traversal
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
│   ├── merge/                  # Merge & Revert Engine
│   │   ├── CMakeLists.txt
│   │   ├── merge_engine.{h,cpp}# 3-way line merge & LCA DAG traversal
│   │   ├── merge.{h,cpp}       # merge command
│   │   └── revert.{h,cpp}      # revert command
│   ├── stash/                  # Working-State Shelving
│   │   ├── CMakeLists.txt
│   │   └── stash.{h,cpp}       # stash command: push, list, pop, drop, show
│   └── remotes/                # Remote Synchronization & Transport Protocol
│       ├── CMakeLists.txt
│       ├── config.{h,cpp}      # .minigit/config INI parser and remote manager
│       ├── transfer.{h,cpp}    # BFS missing-object DAG traversal & ancestry checker
│       ├── remote.{h,cpp}      # remote command: list, add, remove remotes
│       ├── clone.{h,cpp}       # clone command
│       ├── fetch.{h,cpp}       # fetch command
│       ├── push.{h,cpp}        # push command
│       └── pull.{h,cpp}        # pull command
```

---

## MiniGit vs Standard Git

| Dimension | MiniGit | Standard Git |
| :--- | :--- | :--- |
| **Language** | C++20 | C, Shell, Perl |
| **Cryptographic Hash** | SHA-256 (64 hex characters) | SHA-1 (default) / SHA-256 (experimental) |
| **Index Format** | Clean text line format (`<path> <sha256>`) | Binary DIRC (format v2/v3/v4) |
| **Tree Storage** | Text-based sorted entries | Binary mode/path/SHA entries |
| **Diff Engine** | Dynamic programming LCS (`O(M * N)`) | Myers diff algorithm (`O(N * D)`) |
| **Branch Switching** | `minigit switch` and `minigit checkout` | `git switch` and `git checkout` |
| **Submodules & Remotes**| Local remotes protocol (`clone`, `fetch`, `push`, `pull`) | Full local, SSH, Git, HTTP/S protocols |
| **Compression** | zlib deflate compression | zlib deflate compression & Packfiles |

---

## Roadmap

Planned milestones for future MiniGit development:

- [x] **Phase 1: Ignore Rules:** `.minigitignore` glob pattern matching — excludes files from `status` and `add` with support for wildcards, directory patterns (`build/`), and negation (`!pattern`).
- [x] **Phase 2: Tagging:** Lightweight and annotated tags (`minigit tag`) stored under `refs/tags/`; annotated tags are first-class objects in the object database.
- [x] **Phase 3: Three-Way Merging:** Merge base computation, automatic three-way file merge, and conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`).
- [x] **Phase 4: History Rewriting & Safe Undo:** `minigit reset` (`--soft`, `--mixed`, `--hard`) and `minigit revert` (three-way inverse commit application with conflict detection).
- [x] **Phase 5: Stash & Compression:** `minigit stash` working-state shelving (`push`, `list`, `pop`, `drop`, `show`) and zlib deflate loose object compression.
- [x] **Phase 6: Networking & Remotes:** Local filesystem remotes protocol with `clone`, `remote`, `fetch`, `push`, and `pull`.
- [ ] **Phase 7: Packfiles & Delta Compression:** Object database consolidation into binary packfiles (`.pack`), `.idx` fan-out index, and sliding-window byte-level delta compression.
- [ ] **Phase 8: Smart HTTP Remotes:** Remote synchronization over HTTP/HTTPS with bidirectional discover-negotiate-transfer protocol.
- [ ] **Phase 9: Interactive Rebase & Cherry-Pick:** History rewriting (`minigit rebase -i`), commit squashing, amending, and individual commit transplantation (`minigit cherry-pick`).
- [ ] **Phase 10: Worktrees & Submodules:** Multiple linked working trees (`minigit worktree`) and nested repository tracking (`minigit submodule`).

---

## License

This project is licensed under the MIT License — see the [LICENCE](LICENCE) file for details.
