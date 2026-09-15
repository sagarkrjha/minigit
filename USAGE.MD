# MiniGit Command-Line Interface (CLI) Usage Guide

This guide provides end-to-end usage instructions, practical workflow recipes, and a comprehensive command reference for **MiniGit** — a C++20 Git-compatible version control system.

> 📖 For architectural and technical specifications, see [FEATURES.md](FEATURES.md). For project overview and build instructions, see [README.md](README.md).

---

## Table of Contents

- [1. Quick Start & Installation](#1-quick-start--installation)
  - [Download Pre-Built Binaries](#download-pre-built-binaries)
  - [Build from Source](#build-from-source)
- [2. Daily Development Workflow](#2-daily-development-workflow)
  - [Initialize a Repository (`minigit init`)](#initialize-a-repository-minigit-init)
  - [Check Repository Status (`minigit status`)](#check-repository-status-minigit-status)
  - [Stage Changes (`minigit add`)](#stage-changes-minigit-add)
  - [Commit Staged Changes (`minigit commit`)](#commit-staged-changes-minigit-commit)
  - [View Commit History (`minigit log`)](#view-commit-history-minigit-log)
  - [Inspect Differences (`minigit diff`)](#inspect-differences-minigit-diff)
- [3. Branching & Switching](#3-branching--switching)
  - [List Branches (`minigit branch`)](#list-branches-minigit-branch)
  - [Create a Branch (`minigit branch <name>`)](#create-a-branch-minigit-branch-name)
  - [Switch Branches (`minigit switch`)](#switch-branches-minigit-switch)
  - [Delete a Branch (`minigit branch -d`)](#delete-a-branch-minigit-branch--d)
  - [Historic Checkout & Detached HEAD (`minigit checkout`)](#historic-checkout--detached-head-minigit-checkout)
- [4. Milestones & Releases (`minigit tag`)](#4-milestones--releases-minigit-tag)
  - [List Tags](#list-tags)
  - [Create Lightweight Tags](#create-lightweight-tags)
  - [Create Annotated Tags](#create-annotated-tags)
  - [Delete Tags](#delete-tags)
- [5. Merging & Conflict Resolution (`minigit merge`)](#5-merging--conflict-resolution-minigit-merge)
  - [Fast-Forward & 3-Way Merge](#fast-forward--3-way-merge)
  - [Resolving Merge Conflicts](#resolving-merge-conflicts)
- [6. Undoing & History Rewriting](#6-undoing--history-rewriting)
  - [Undo Changes with Reset (`minigit reset`)](#undo-changes-with-reset-minigit-reset)
  - [Invert Commits with Revert (`minigit revert`)](#invert-commits-with-revert-minigit-revert)
- [7. Shelving Work with Stash (`minigit stash`)](#7-shelving-work-with-stash-minigit-stash)
  - [Save Uncommitted Work (`push`)](#save-uncommitted-work-push)
  - [List Stashes (`list`)](#list-stashes-list)
  - [Inspect Stash Contents (`show`)](#inspect-stash-contents-show)
  - [Restore Stashed Changes (`pop`)](#restore-stashed-changes-pop)
  - [Discard Stash Entries (`drop`)](#discard-stash-entries-drop)
- [8. Remote Repositories & Synchronization](#8-remote-repositories--synchronization)
  - [Clone a Repository (`minigit clone`)](#clone-a-repository-minigit-clone)
  - [Manage Remotes (`minigit remote`)](#manage-remotes-minigit-remote)
  - [Fetch Updates (`minigit fetch`)](#fetch-updates-minigit-fetch)
  - [Push Commits (`minigit push`)](#push-commits-minigit-push)
  - [Pull & Fast-Forward (`minigit pull`)](#pull--fast-forward-minigit-pull)
- [9. Ignoring Files (`.minigitignore`)](#9-ignoring-files-minigitignore)
- [10. Low-Level Plumbing Commands](#10-low-level-plumbing-commands)
  - [Compute Object Hashes (`minigit hash-object`)](#compute-object-hashes-minigit-hash-object)
  - [Write Staging Area to Tree (`minigit write-tree`)](#write-staging-area-to-tree-minigit-write-tree)
  - [Inspect Stored Objects (`minigit cat-file`)](#inspect-stored-objects-minigit-cat-file)
- [11. Command Summary & Cheat Sheet](#11-command-summary--cheat-sheet)

---

## 1. Quick Start & Installation

### Download Pre-Built Binaries

Pre-compiled native standalone executables are automatically built, verified, and published for each release.

| OS | Direct Download Link | Execution | Downloads |
| :--- | :--- | :--- | :--- |
| **Windows** | [`minigit.exe`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) | `.\minigit.exe <command>` | [![Windows Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit.exe?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) |
| **Linux** | [`minigit-linux`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) | `chmod +x minigit-linux && ./minigit-linux <command>` | [![Linux Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-linux?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) |
| **macOS** | [`minigit-macos`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) | `chmod +x minigit-macos && ./minigit-macos <command>` | [![macOS Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-macos?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) |

### Build from Source

Prerequisites: CMake 3.20+, C++20 compiler (GCC 11+, Clang 13+, MSVC 2019+), OpenSSL, zlib.

```bash
# Clone the repository
git clone https://github.com/sagarkrjha/minigit.git
cd minigit

# Configure and compile (Release mode)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Executable is built in ./build/minigit (or ./build/Release/minigit.exe on Windows)
```

---

## 2. Daily Development Workflow

### Initialize a Repository (`minigit init`)

Initializes a new MiniGit repository in the current working directory, creating the internal `.minigit` hierarchy (`objects/`, `refs/heads/`, `refs/tags/`, `HEAD`, `config`, and `index`).

```bash
# Initialize in current directory
minigit init
```

*Output:*
```text
Initialized empty mini_git repository in C:/dev/my-project/.minigit/
```

*Running `minigit init` in an already initialized repository will safely report reinitialization without overwriting existing commits.*

---

### Check Repository Status (`minigit status`)

Compares the three state trees (**Working Directory**, **Staging Index**, and **HEAD commit**) to display:
- Changes staged for commit (`new file`)
- Changes not staged for commit (`modified`, `deleted`)
- Untracked files (excluding `.minigitignore` patterns)

```bash
minigit status
```

*Example Output:*
```text
On branch main

Changes to be committed:
	new file:   README.md
	new file:   src/main.cpp

Changes not staged for commit:
	modified:   CMakeLists.txt
	deleted:    notes.txt

Untracked files:
	tests/test_parser.cpp
```

When no differences exist:
```text
On branch main

nothing to commit, working tree clean
```

---

### Stage Changes (`minigit add`)

Stages new files, file modifications, and file deletions into `.minigit/index`, computing cryptographic SHA-256 blobs and writing them to `.minigit/objects/`.

```bash
# 1. Stage the entire working tree (new, modified, and deleted files)
minigit add .

# 2. Stage specific files
minigit add src/main.cpp include/header.h

# 3. Stage an entire directory recursively
minigit add src/

# 4. Stage deletion of a tracked file that was removed from disk
rm old_file.txt
minigit add old_file.txt
# (or simply run `minigit add .`)
```

**Key Behaviors:**
- **Whole Directory / `.`**: Recursively visits all subdirectories, stages new/modified files, and removes deleted tracked files from the index.
- **Subdirectory Isolation**: When invoked inside a subdirectory (`cd src && minigit add .`), staging is strictly scoped to that subdirectory.
- **Ignore Rules**: Files and folders matching `.minigitignore` are silently skipped during recursive walks.
- **Internal Protection**: Never stages `.minigit` or `.git` internal folders.

---

### Commit Staged Changes (`minigit commit`)

Serializes the current index into an immutable `Tree` object, creates a `Commit` object pointing to the parent commit and author metadata, and advances the current branch reference.

```bash
# Commit with message (default author: "MiniGit User <user@minigit>")
minigit commit -m "Implement core parsing engine"

# Commit with explicit author attribution
minigit commit -m "Add unit tests" --author "Jane Doe <jane@example.com>"
```

*Output:*
```text
[7e9a12c] Implement core parsing engine
```
*(On root commits, outputs `[(root-commit) 7e9a12c]`)*

---

### View Commit History (`minigit log`)

Displays the linear history starting from `HEAD` and walking backward through first-parent pointers to the root commit.

```bash
minigit log
```

*Example Output:*
```text
commit 7e9a12cf4603951239c4f4244f7d4bb21a71997d9178ad9902636a04a6ee1039
Author: Jane Doe <jane@example.com>
Date:   1773322800

    Add unit tests

commit 1a0b3c58342dc2145b20756e4c7ba987e9e6a0d0d4638708c3525287f3942007
Author: MiniGit User <user@minigit>
Date:   1773319200

    Initial commit
```

---

### Inspect Differences (`minigit diff`)

Uses dynamic programming Longest Common Subsequence (LCS) to generate unified diff patches (`---` / `+++` / `@@ -x,y +x,y @@`).

```bash
# 1. Unstaged changes: Working Directory vs. Staging Index
minigit diff

# 2. Staged changes: Staging Index vs. HEAD Commit
minigit diff --cached
# or
minigit diff --staged

# 3. Restrict diff to specific file or directory prefix
minigit diff src/main.cpp
minigit diff --cached src/
```

*Example Output:*
```diff
diff --minigit a/src/main.cpp b/src/main.cpp
--- a/src/main.cpp
+++ b/src/main.cpp
@@ -10,3 +10,4 @@
 int main() {
+    std::cout << "MiniGit Version 1.0\n";
     return 0;
 }
```

---

## 3. Branching & Switching

### List Branches (`minigit branch`)

Lists all local branches under `.minigit/refs/heads/`, marking the currently checked-out branch with an asterisk (`* `).

```bash
minigit branch
```

*Output:*
```text
* main
  feature/parser
  release/v1.0
```

---

### Create a Branch (`minigit branch <name>`)

Creates a new branch reference pointing to the current `HEAD` commit. Fails if the branch already exists or if `HEAD` has no commits.

```bash
minigit branch feature/auth
```
*Output:*
```text
Created branch 'feature/auth' at 7e9a12c
```

---

### Switch Branches (`minigit switch`)

Safely restores the working directory and index to match the target branch.

```bash
# Switch to an existing branch
minigit switch feature/auth

# Create AND switch to a new branch in a single command (-c)
minigit switch -c feature/payments
```

*Output:*
```text
Created branch 'feature/payments' at 7e9a12c
Switched to branch 'feature/payments'
```

---

### Delete a Branch (`minigit branch -d`)

Removes a branch reference. MiniGit safeguards against deleting the currently active branch.

```bash
minigit branch -d feature/auth
```
*Output:*
```text
Deleted branch feature/auth
```

---

### Historic Checkout & Detached HEAD (`minigit checkout`)

Restores the working directory and staging index to the state of a branch or any specific 64-character SHA commit.

```bash
# 1. Checkout a branch
minigit checkout main

# 2. Checkout a specific historical commit SHA (Detached HEAD state)
minigit checkout 1a0b3c58342dc2145b20756e4c7ba987e9e6a0d0d4638708c3525287f3942007
```

*Output:*
```text
HEAD is now at 1a0b3c5 Initial commit
```

*To return from detached HEAD, switch back to a named branch:*
```bash
minigit switch main
```

---

## 4. Milestones & Releases (`minigit tag`)

MiniGit supports both lightweight pointers and first-class annotated tag objects.

### List Tags
```bash
minigit tag
```
*Output:*
```text
v1.0.0
v1.1.0-release
```

### Create Lightweight Tags
A lightweight tag is a named reference in `.minigit/refs/tags/` pointing directly to the current commit SHA:
```bash
minigit tag v1.0.0
```

### Create Annotated Tags
Annotated tags are stored in `.minigit/objects/` containing tagger identity, timestamp, and an annotation message:
```bash
minigit tag -a v1.1.0 -m "Production release milestone 1.1.0"
```

### Delete Tags
```bash
minigit tag -d v1.0.0
```
*Output:*
```text
Deleted tag v1.0.0
```

---

## 5. Merging & Conflict Resolution (`minigit merge`)

Performs a branch merge into the currently checked-out branch.

```bash
# Syntax: minigit merge <target-branch> [--author <author>]
minigit merge feature/parser
```

### Fast-Forward & 3-Way Merge

1. **Fast-Forward**: If current `HEAD` is a direct ancestor of `<target-branch>`, MiniGit advances the branch pointer directly without creating an extra commit:
   ```text
   Fast-forward
   ```
2. **Three-Way Merge**: If histories have diverged, MiniGit computes the Lowest Common Ancestor (LCA) merge base and executes line-level 3-way merging. On success, an automated merge commit with two parents is created:
   ```text
   Merge made by the 'recursive' strategy.
   [5d24932] Merge branch 'feature/parser' into main
   ```

### Resolving Merge Conflicts

If conflicting edits occur in the same file region:
```text
CONFLICT (content): Merge conflict in src/parser.cpp

Automatic merge failed; fix conflicts and then commit the result.
```

MiniGit writes conflict markers directly into the files:
```cpp
<<<<<<< main
int timeout = 5000;
=======
int timeout = 10000;
>>>>>>> feature/parser
```

**Resolution Workflow:**
1. Open conflicted files in an editor and select the desired code.
2. Remove marker lines (`<<<<<<<`, `=======`, `>>>>>>>`).
3. Stage the resolved files:
   ```bash
   minigit add src/parser.cpp
   ```
4. Finalize the merge commit:
   ```bash
   minigit commit -m "Merge branch 'feature/parser' into main with conflict resolution"
   ```

---

## 6. Undoing & History Rewriting

### Undo Changes with Reset (`minigit reset`)

Rolls back `HEAD` to a target commit or branch, offering three levels of depth:

```bash
# 1. Soft Reset: Move HEAD only. Staging area (index) and working directory untouched.
minigit reset --soft <commit-sha>

# 2. Mixed Reset (Default): Move HEAD and reset staging area. Working tree files untouched.
minigit reset <commit-sha>
minigit reset --mixed <commit-sha>

# 3. Hard Reset: Move HEAD, reset staging area, AND revert all working directory files.
# WARNING: Discards all uncommitted changes permanently.
minigit reset --hard <commit-sha>
```

---

### Invert Commits with Revert (`minigit revert`)

Inverts the changes introduced by `<commit>` by computing an inverse three-way merge and committing the reverse patch on top of current `HEAD`. Safe for shared/public branches.

```bash
minigit revert 7e9a12cf4603951239c4f4244f7d4bb21a71997d9178ad9902636a04a6ee1039
```

*Output:*
```text
[f00c6db] Revert "Implement core parsing engine"
```

---

## 7. Shelving Work with Stash (`minigit stash`)

Temporarily shelves uncommitted changes (both staged and unstaged) into a stash stack, reverting the working directory back to `HEAD`.

```bash
# 1. Save uncommitted changes
minigit stash
# or
minigit stash push

# 2. List all saved stashes
minigit stash list
# Output: stash@{0}: WIP on main: 7e9a12c Add unit tests

# 3. Inspect files modified in a stash entry
minigit stash show stash@{0}

# 4. Restore stashed changes and remove entry from stash stack
minigit stash pop
minigit stash pop stash@{0}

# 5. Discard a stash entry without applying it
minigit stash drop stash@{0}
```

---

## 8. Remote Repositories & Synchronization

MiniGit includes full local-filesystem synchronization protocol commands.

### Clone a Repository (`minigit clone`)

Clones an existing repository into a target directory, sets up `origin` tracking in `.minigit/config`, and checks out `HEAD`.

```bash
minigit clone C:/projects/upstream downstream_repo
cd downstream_repo
```

---

### Manage Remotes (`minigit remote`)

Inspect and manage remote repository endpoints configured in `.minigit/config`:

```bash
# List remote names
minigit remote

# List remotes with URLs (verbose)
minigit remote -v

# Add a remote
minigit remote add backup C:/backups/minigit_repo

# Remove a remote
minigit remote remove backup
```

---

### Fetch Updates (`minigit fetch`)

Downloads missing commits, trees, and blobs from a remote and updates remote-tracking references under `.minigit/refs/remotes/<remote>/<branch>` without modifying local branches:

```bash
minigit fetch origin
```

---

### Push Commits (`minigit push`)

Transfers missing objects to the remote and advances the remote branch reference using fast-forward verification:

```bash
# Push current branch to origin
minigit push origin main
```

---

### Pull & Fast-Forward (`minigit pull`)

Fetches latest objects from the remote and fast-forwards the active branch:

```bash
minigit pull origin main
```

---

## 9. Ignoring Files (`.minigitignore`)

Place a `.minigitignore` file in your repository root to prevent untracked files from appearing in `minigit status` and `minigit add .`:

```gitignore
# Comments start with '#'

# Ignore all files with specific extensions
*.o
*.obj
*.exe
*.log

# Ignore a directory and all contained files
build/
bin/
.vscode/

# Ignore a specific path relative to repo root
temp/cache.json

# Negate an ignore pattern (un-ignore)
!important.log
```

---

## 10. Low-Level Plumbing Commands

Plumbing commands provide low-level access to the Content-Addressable Storage (CAS) engine.

### Compute Object Hashes (`minigit hash-object`)

Calculates the SHA-256 hash of a file as a blob envelope (`blob <size>\0<content>`).

```bash
# Print SHA-256 without writing to database
minigit hash-object main.cpp

# Compute SHA-256 AND persist to .minigit/objects/ (-w)
minigit hash-object -w main.cpp
# Output: 487b32f9bf2dd4f923b77382025e6834164b85c18a204620f4c3de436894c77c
```

---

### Write Staging Area to Tree (`minigit write-tree`)

Takes the current contents of `.minigit/index`, creates a serialized `Tree` object, writes it to `.minigit/objects/`, and prints its SHA-256:

```bash
minigit write-tree
# Output: 9e5c46b9a89d1469e5f583856b3e34b9b4bc0a2b083b4827051a89c938be985e
```

---

### Inspect Stored Objects (`minigit cat-file`)

Inspects any object (blob, tree, commit, or tag) in the object store by its SHA-256 hash:

| Flag | Description |
| :--- | :--- |
| `-t` | Print the object type (`blob`, `tree`, `commit`, `tag`). |
| `-s` | Print the object payload size in bytes. |
| `-p` | Pretty-print formatted contents according to type. |

```bash
SHA="487b32f9bf2dd4f923b77382025e6834164b85c18a204620f4c3de436894c77c"

# Inspect type
minigit cat-file -t $SHA
# blob

# Inspect size
minigit cat-file -s $SHA
# 42

# Pretty-print content
minigit cat-file -p $SHA
```

---

## 11. Command Summary & Cheat Sheet

| Command | Synopsis | Description |
| :--- | :--- | :--- |
| `init` | `minigit init` | Initialize a new repository or reinitialize an existing one. |
| `status` | `minigit status` | Report status across Working Tree, Index, and HEAD. |
| `add` | `minigit add (<file>\|<dir>\|.) ...` | Stage files, directories, or entire working tree into index. |
| `commit` | `minigit commit -m <msg> [--author <a>]` | Record staged snapshot into an immutable commit object. |
| `log` | `minigit log` | Traverse linear commit history backward from HEAD. |
| `diff` | `minigit diff [--cached] [<path>...]` | Show line-level unified diffs (unstaged or staged). |
| `branch` | `minigit branch [-d <name>] [<name>]` | List, create, or delete branches. |
| `switch` | `minigit switch [-c] <branch>` | Switch active branch, optionally creating it first with `-c`. |
| `checkout`| `minigit checkout <branch-or-sha>` | Restore working files and index to a branch or commit. |
| `tag` | `minigit tag [-a -m msg \| -d] [<name>]` | List, create (lightweight / annotated), or delete tags. |
| `reset` | `minigit reset [--soft\|--mixed\|--hard] <sha>`| Roll back HEAD, index, and/or working tree. |
| `merge` | `minigit merge <branch> [--author <a>]` | Merge a branch into HEAD with LCA 3-way merge engine. |
| `revert` | `minigit revert <commit> [--author <a>]` | Create a new commit inverting changes of a previous commit. |
| `stash` | `minigit stash [push\|list\|pop\|drop\|show]` | Shelve uncommitted modifications without committing. |
| `remote` | `minigit remote [add\|remove\|-v] [<args>]` | Inspect or manage remote repository aliases. |
| `clone` | `minigit clone <src> [<dest>]` | Clone a repository, set up tracking, and check out HEAD. |
| `fetch` | `minigit fetch [<remote>]` | Download objects and update remote-tracking references. |
| `push` | `minigit push [<remote> [<branch>]]` | Upload objects and advance remote branch references. |
| `pull` | `minigit pull [<remote> [<branch>]]` | Fetch and fast-forward the current branch with remote updates. |
| `hash-object` | `minigit hash-object [-w] <file>` | Compute SHA-256 for a file; optionally persist as blob. |
| `write-tree` | `minigit write-tree` | Serialize current index entries into a tree object. |
| `cat-file` | `minigit cat-file (-t\|-s\|-p) <sha>` | Inspect object type, size, or pretty-print contents. |
