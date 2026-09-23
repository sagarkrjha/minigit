# MiniGit Command-Line Interface (CLI) Usage Guide

This guide provides end-to-end usage instructions, practical workflow recipes, and a comprehensive command reference for **MiniGit** — a C++20 Git-compatible version control system.

> 📖 For architectural and technical specifications, see [FEATURES.md](FEATURES.md). For project overview and build instructions, see [README.md](README.md).

---

## Table of Contents

- [1. Quick Start & Installation](#1-quick-start--installation)
  - [Download Pre-Built Binaries](#download-pre-built-binaries)
  - [Permission-Based Installation (`minigit install`)](#permission-based-installation-minigit-install)
  - [Build from Source](#build-from-source)
- [2. Daily Development Workflow](#2-daily-development-workflow)
  - [Initialize a Repository (`minigit init`)](#initialize-a-repository-minigit-init)
  - [Check Repository Status (`minigit status`)](#check-repository-status-minigit-status)
  - [Stage Changes (`minigit add`)](#stage-changes-minigit-add)
  - [Commit Staged Changes (`minigit commit`)](#commit-staged-changes-minigit-commit)
  - [View Commit History (`minigit log`)](#view-commit-history-minigit-log)
  - [Inspect Differences (`minigit diff`)](#inspect-differences-minigit-diff)
  - [Inspect Objects & Commits (`minigit show`)](#inspect-objects--commits-minigit-show)
  - [Clean Untracked Files and Directories (`minigit clean`)](#clean-untracked-files-and-directories-minigit-clean)
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
  - [Transplant Commits with Cherry-Pick (`minigit cherry-pick`)](#transplant-commits-with-cherry-pick-minigit-cherry-pick)
  - [Replay Linear History with Rebase (`minigit rebase`)](#replay-linear-history-with-rebase-minigit-rebase)
- [7. Shelving Work with Stash (`minigit stash`)](#7-shelving-work-with-stash-minigit-stash)
  - [Save Uncommitted Work (`minigit stash push`)](#save-uncommitted-work-minigit-stash-push)
  - [List Stashes (`minigit stash list`)](#list-stashes-minigit-stash-list)
  - [Inspect Stash Contents (`minigit stash show`)](#inspect-stash-contents-minigit-stash-show)
  - [Restore Stashed Changes (`minigit stash pop`)](#restore-stashed-changes-minigit-stash-pop)
  - [Discard Stash Entries (`minigit stash drop`)](#discard-stash-entries-minigit-stash-drop)
- [8. Multiple Working Trees (`minigit worktree`)](#8-multiple-working-trees-minigit-worktree)
  - [Add a Working Tree (`minigit worktree add`)](#add-a-working-tree-minigit-worktree-add)
  - [List Active Working Trees (`minigit worktree list`)](#list-active-working-trees-minigit-worktree-list)
  - [Lock and Unlock Working Trees (`minigit worktree lock` / `unlock`)](#lock-and-unlock-working-trees-minigit-worktree-lock--unlock)
  - [Move a Working Tree (`minigit worktree move`)](#move-a-working-tree-minigit-worktree-move)
  - [Remove a Working Tree (`minigit worktree remove`)](#remove-a-working-tree-minigit-worktree-remove)
  - [Prune Stale Working Trees (`minigit worktree prune`)](#prune-stale-working-trees-minigit-worktree-prune)
- [9. Submodules (`minigit submodule`)](#9-submodules-minigit-submodule)
  - [Add a Submodule (`minigit submodule add`)](#add-a-submodule-minigit-submodule-add)
  - [Check Submodule Status (`minigit submodule status`)](#check-submodule-status-minigit-submodule-status)
  - [Initialize and Update Submodules (`minigit submodule init` / `update`)](#initialize-and-update-submodules-minigit-submodule-init--update)
  - [Run Commands in All Submodules (`minigit submodule foreach`)](#run-commands-in-all-submodules-minigit-submodule-foreach)
  - [Deinitialize Submodules (`deinit`)](#deinitialize-submodules-minigit-submodule-deinit)
  - [Synchronize Remote URLs (`minigit submodule sync`)](#synchronize-remote-urls-minigit-submodule-sync)
  - [Inspect Differences (`minigit submodule summary`)](#inspect-submodule-commit-differences-minigit-submodule-summary)
- [10. Binary Search Debugging (`minigit bisect`)](#10-binary-search-debugging-minigit-bisect)
  - [Start a Bisection Session (`minigit bisect start`)](#start-a-bisection-session-minigit-bisect-start)
  - [Mark Commits as Bad, Good, or Skip (`bad` / `good` / `skip`)](#mark-commits-as-bad-good-or-skip-minigit-bisect-bad--good--skip)
  - [Automated Bisection Runner (`minigit bisect run`)](#automated-bisection-runner-minigit-bisect-run)
  - [Session History & Replay (`log` / `replay`)](#session-history--replay-minigit-bisect-log--replay)
  - [Custom Terms (`minigit bisect terms`)](#custom-terms-minigit-bisect-terms)
  - [Finish and Clean Up (`minigit bisect reset`)](#finish-and-clean-up-minigit-bisect-reset)
- [11. Remote Repositories & Synchronization](#11-remote-repositories--synchronization)
  - [Clone a Repository (`minigit clone`)](#clone-a-repository-minigit-clone)
  - [Manage Remotes (`minigit remote`)](#manage-remotes-minigit-remote)
  - [Fetch Updates (`minigit fetch`)](#fetch-updates-minigit-fetch)
  - [Push Commits (`minigit push`)](#push-commits-minigit-push)
  - [Pull & Fast-Forward (`minigit pull`)](#pull--fast-forward-minigit-pull)
  - [Test Environment SSL Verification](#test-environment-ssl-verification)
- [12. Ignoring Files (`.minigitignore`)](#12-ignoring-files-minigitignore)
- [13. Low-Level Plumbing Commands](#13-low-level-plumbing-commands)
  - [Compute Object Hashes (`minigit hash-object`)](#compute-object-hashes-minigit-hash-object)
  - [Write Staging Area to Tree (`minigit write-tree`)](#write-staging-area-to-tree-minigit-write-tree)
  - [Inspect Stored Objects (`minigit cat-file`)](#inspect-stored-objects-minigit-cat-file)
  - [List Staged & Working Tree Files (`minigit ls-files`)](#list-staged--working-tree-files-minigit-ls-files)
  - [Inspect Tree Objects (`minigit ls-tree`)](#inspect-tree-objects-minigit-ls-tree)
  - [Packfile Maintenance & Compaction (`minigit repack`)](#packfile-maintenance--compaction-minigit-repack)
  - [Verify Packfiles (`minigit verify-pack`)](#verify-packfiles-minigit-verify-pack)
  - [Display Version Information (`minigit version`)](#display-version-information-minigit-version)
- [14. Checking for Updates & Self-Update (`minigit update`)](#14-checking-for-updates--self-update-minigit-update)
  - [Check for Available Updates (`minigit update --check`)](#141-check-for-available-updates)
  - [Download and Apply Self-Update (`minigit update`)](#142-download-and-apply-self-update)
  - [Terminal Update Notification Banner](#143-terminal-update-notification-banner)
- [15. Command Summary & Cheat Sheet](#15-command-summary--cheat-sheet)

---

## 1. Quick Start & Installation

### Download Pre-Built Binaries

Pre-compiled native standalone executables are automatically built, verified, and published for each release.

| OS | Direct Download Link | Execution | Downloads |
| :--- | :--- | :--- | :--- |
| **Windows** | [`minigit.exe`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) | `.\minigit.exe <command>` | [![Windows Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit.exe?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe) |
| **Linux** | [`minigit-linux`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) | `chmod +x minigit-linux && ./minigit-linux <command>` | [![Linux Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-linux?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux) |
| **macOS** | [`minigit-macos`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) | `chmod +x minigit-macos && ./minigit-macos <command>` | [![macOS Downloads](https://img.shields.io/github/downloads/sagarkrjha/minigit/latest/minigit-macos?style=flat-square&label=downloads&color=blue)](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos) |

### Permission-Based Installation (`minigit install`)

MiniGit includes a built-in porcelain installer modeled directly after Git for Windows. It structures the installation root into `cmd/`, `bin/`, and `etc/` directories, configures your environment `PATH` pointing to `cmd/` (preventing tool name collisions), registers MiniGit in Windows **Installed Apps (Add/Remove Programs)**, and adds the **"Open MiniGit Prompt Here"** context menu to Windows Explorer.

#### Directory Layout

```text
<InstallRoot>/
├── cmd/
│   └── minigit.exe        ← Added to PATH (matching Git for Windows <Git>\cmd)
├── bin/
│   └── minigit.exe        ← Core binary
└── etc/
    ├── minigitconfig      ← System-wide configuration ([core] autocrlf = true)
    └── templates/         ← Default repository templates
```

#### 1. Per-User Installation (Default for Standard Users)
Installs MiniGit for the current user into `%LOCALAPPDATA%\Programs\MiniGit` without requiring administrative rights:

```powershell
.\minigit.exe install --user
```

*Output:*
```text
Installing minigit (v1.11.0)...
Installation scope:  User (Current User)
Destination root:    C:\Users\username\AppData\Local\Programs\MiniGit
CLI command binary:  C:\Users\username\AppData\Local\Programs\MiniGit\cmd\minigit.exe
Core binary:         C:\Users\username\AppData\Local\Programs\MiniGit\bin\minigit.exe
Environment PATH:    Added <InstallDir>\cmd to PATH
Apps & Features:     Registered in Windows Installed Apps
Explorer Menu:       Registered 'Open MiniGit Prompt Here'
minigit installed successfully!
```

#### 2. System-Wide Installation (Requires Administrator Privileges)
Installs MiniGit machine-wide for all users into `%ProgramFiles%\MiniGit`:

```powershell
.\minigit.exe install --system
```

*When run from a standard non-elevated prompt, MiniGit automatically requests Administrator privileges via Windows UAC dialog (`runas`). Once approved, the installation proceeds seamlessly, configures the machine PATH, and registers system-level uninstall and context menu entries.*

#### 3. Custom Installation Directory
You can specify an arbitrary target directory with `--dir`:

```powershell
.\minigit.exe install --dir "D:\tools\MiniGit"
```

#### 4. Skipping Environment PATH or Context Menu
- Use `--no-path` to deploy without altering user or system `PATH`:
  ```powershell
  .\minigit.exe install --no-path
  ```
- Use `--no-context-menu` to omit the Windows Explorer right-click context menu:
  ```powershell
  .\minigit.exe install --no-context-menu
  ```

#### 5. Uninstallation
To cleanly remove MiniGit, delete its `cmd/` entry from `PATH`, and unregister Windows Installed Apps and Explorer context menus:

```powershell
minigit install --uninstall
```
*(You can also uninstall directly from Windows Settings > Installed Apps / Add or Remove Programs).*

---

### Build from Source

Prerequisites: CMake 3.20+, C++20 compiler (GCC 11+, Clang 13+, MSVC 2019+), OpenSSL, zlib, libcurl.

```bash
# Clone the repository
git clone https://github.com/sagarkrjha/minigit.git
cd minigit

# Configure and compile (Release mode)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Executable is built in ./build/minigit (or ./build/minigit.exe on Windows)
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

### Inspect Objects & Commits (`minigit show`)

Displays commit metadata alongside the unified diff against its parent commit, inspects tags, and directly examines trees or blobs.

```bash
# 1. Inspect the HEAD commit and its changes
minigit show

# 2. Inspect a specific commit, branch, or tag
minigit show feature/analytics
minigit show v1.0.0
minigit show 7e9a12c

# 3. Ancestry navigation (~N or ^)
minigit show HEAD~1
minigit show main^

# 4. View diffstat summary or modified filenames only
minigit show --stat
minigit show --name-only 7e9a12c

# 5. Inspect tree or blob objects directly
minigit show <tree-sha>
minigit show <blob-sha>
```

*Example Output (Commit):*
```text
commit 7e9a12cf4603951239c4f4244f7d4bb21a71997d9178ad9902636a04a6ee1039
Author: Jane Doe <jane@example.com>
Date:   1773322800

    Add unit tests

diff --minigit a/tests/test_service.cpp b/tests/test_service.cpp
--- a//dev/null
+++ b/tests/test_service.cpp
@@ -1,0 +1,5 @@
+#include "service.h"
+...
```

---

### Clean Untracked Files and Directories (`minigit clean`)

Removes untracked files and directories from the working directory. Requires `-f` (`--force`) or `-n` (`--dry-run`) to prevent accidental data loss.

```bash
# 1. Preview untracked files that would be removed (dry-run)
minigit clean -n

# 2. Force removal of untracked files
minigit clean -f

# 3. Remove untracked directories as well (-d)
minigit clean -fd

# 4. Remove all untracked files including ignored ones (-x)
minigit clean -fx

# 5. Restrict cleaning to a specific path or prefix
minigit clean -f src/temp/
```

*Example Output:*
```text
Would remove scratch.log
Would remove temp_build/
Removing scratch.log
Removing temp_build/
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

### Transplant Commits with Cherry-Pick (`minigit cherry-pick`)

Selectively applies the exact changes introduced by an existing commit onto the current working branch, creating a new commit that preserves the original commit message and author identity.

```bash
# 1. Cherry-pick by branch name or commit SHA
minigit cherry-pick feature/analytics
# or
minigit cherry-pick 7b1c402

# 2. Stage changes without committing (--no-commit / -n)
minigit cherry-pick -n hotfix/db-patch

# 3. Override author identity
minigit cherry-pick 7b1c402 --author "Alice Developer <alice@example.com>"

# 4. Cherry-pick from a merge commit specifying mainline parent
minigit cherry-pick -m 1 <merge-commit-sha>
```

*Output:*
```text
[main 7b1c402] Fix query timeout in payment gateway
```

**Conflict Handling:**
If conflicting changes exist, conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) are inserted into the file and the cherry-pick pauses:
```text
CONFLICT (content): Merge conflict in src/service.cpp
error: could not apply 7b1c402... Fix query timeout
hint: after resolving the conflicts, mark the corrected paths
hint: with 'minigit add <paths>' or 'minigit rm <paths>'
hint: and commit the result with 'minigit commit'
```

### Replay Linear History with Rebase (`minigit rebase`)

Replays commits from the current branch onto an upstream branch or commit, producing a clean, linear project history without merge commits.

```bash
# 1. Standard Rebase: Replay current branch commits onto upstream
minigit switch feature/payment
minigit rebase main

# 2. Rebase onto a different base branch
minigit rebase --onto main feature/legacy-auth

# 3. Continue rebase after resolving conflict
minigit add <resolved-files>
minigit rebase --continue

# 4. Skip the currently conflicted commit
minigit rebase --skip

# 5. Abort rebase and restore original branch and working tree
minigit rebase --abort
```

*Example Clean Rebase Output:*
```text
First, rewinding head to replay your work on top of it...
Applying: Add credit card tokenization
Applying: Integrate Stripe webhook handler
Successfully rebased and updated refs/heads/feature/payment.
```

*Conflict Handling Workflow:*
When rebase encounters a conflict during commit replay:
```text
Applying: Add credit card tokenization
CONFLICT (content): Merge conflict in src/payment.cpp
error: could not apply 3a7b1c4... Add credit card tokenization
hint: Resolve all conflicts manually, mark them as resolved with
hint: "minigit add <file>", then run "minigit rebase --continue".
hint: You can instead skip this commit: run "minigit rebase --skip".
hint: To abort and get back to the state before "minigit rebase", run "minigit rebase --abort".
```
1. Resolve the conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) in the conflicted file.
2. Stage the resolution:
   ```bash
   minigit add src/payment.cpp
   ```
3. Resume replaying the remaining commits:
   ```bash
   minigit rebase --continue
   ```
4. If you decide the commit should be abandoned or was already applied upstream, run `minigit rebase --skip`. To return safely to your branch tip prior to rebasing, run `minigit rebase --abort`.

---

## 7. Shelving Work with Stash (`minigit stash`)

Temporarily shelves uncommitted changes (both staged and unstaged) into a stash stack, reverting the working directory back to `HEAD`.

### Save Uncommitted Work (`minigit stash push`)

Shelve local changes and restore a clean HEAD working tree:

```bash
# Save uncommitted changes (push is the default)
minigit stash
# or explicitly
minigit stash push
```

### List Stashes (`minigit stash list`)

Inspect saved stashes in the stash stack:

```bash
minigit stash list
# Output: stash@{0}: WIP on main: 7e9a12c Add unit tests
```

### Inspect Stash Contents (`minigit stash show`)

Inspect files modified inside a stash entry:

```bash
minigit stash show stash@{0}
```

### Restore Stashed Changes (`minigit stash pop`)

Restore modifications from a stash and remove the entry from the stash stack:

```bash
# Pop most recent stash (stash@{0})
minigit stash pop

# Or pop a specific stash entry
minigit stash pop stash@{0}
```

### Discard Stash Entries (`minigit stash drop`)

Discard a stash entry without applying it to the working directory:

```bash
minigit stash drop stash@{0}
```

---

## 8. Multiple Working Trees (`minigit worktree`)

MiniGit allows maintaining multiple working directories attached to a single repository simultaneously. This eliminates the need to stash, commit incomplete work, or re-clone repositories when switching context to a hotfix or parallel feature branch.

All linked worktrees share the central Content-Addressable Storage (`objects/`), packfiles, and reference namespace (`refs/heads/`, `refs/tags/`), while possessing an independent working tree directory, staging area (`index`), and active `HEAD`.

### Add a Working Tree (`minigit worktree add`)

Creates a new linked working tree at the specified path and checks out the requested branch or commit:

```bash
# 1. Create a worktree with a new branch (-b)
minigit worktree add ../feature-auth -b feature/oauth2

# 2. Reset and create a branch (-B)
minigit worktree add ../hotfix -B hotfix/critical-fix

# 3. Create a worktree with a detached HEAD (--detach)
minigit worktree add --detach ../perf-test v1.4.0

# 4. Create a worktree checking out an existing branch
minigit worktree add ../staging-env staging
```

> [!IMPORTANT]
> **Branch Exclusivity**: A local branch can only be checked out in one working tree at a time. If you attempt to check out a branch already active in another tree, MiniGit blocks the operation:
> ```text
> fatal: 'main' is already checked out at 'C:/projects/repo'
> ```

---

### List Active Working Trees (`minigit worktree list`)

Lists all attached working trees, their HEAD commits, active branches, and lock status:

```bash
# Human-readable table
minigit worktree list
```
*Output:*
```text
C:/projects/myapp              8b4c291 [main]
C:/projects/feature-auth       a1b2c3d [feature/oauth2]
C:/projects/hotfix             8b4c291 (detached HEAD)
```

```bash
# Machine-readable porcelain output
minigit worktree list --porcelain
```
*Output:*
```text
worktree C:/projects/myapp
HEAD 8b4c2910daed6132b0e6aeddc76117eb385dce51c015bbad1e25e28751801577
branch refs/heads/main

worktree C:/projects/feature-auth
HEAD a1b2c3d4e5f6...
branch refs/heads/feature/oauth2
```

---

### Lock and Unlock Working Trees (`minigit worktree lock / unlock`)

Prevents linked worktree metadata from being pruned if the working tree directory is located on a removable drive, temporary network share, or undergoing maintenance:

```bash
# 1. Lock a worktree with an optional explanation
minigit worktree lock --reason "Mounted on external SSD" feature-auth

# 2. Unlock a worktree
minigit worktree unlock feature-auth
```

---

### Move a Working Tree (`minigit worktree move`)

Relocates an existing linked working tree to a new filesystem path without losing administrative linkage or uncommitted files:

```bash
minigit worktree move feature-auth ../feature-auth-v2
```

---

### Remove a Working Tree (`minigit worktree remove`)

Deletes the working tree files and cleans up the administrative metadata in the main repository:

```bash
# 1. Remove clean worktree
minigit worktree remove ../feature-auth

# 2. Force removal if untracked or uncommitted changes exist
minigit worktree remove -f ../feature-auth
```

---

### Prune Stale Working Trees (`minigit worktree prune`)

Scans the administrative directory (`.minigit/worktrees/`) and removes records for worktree directories that have been manually deleted from disk (unless locked):

```bash
# 1. Dry run: preview what would be pruned
minigit worktree prune -n

# 2. Verbose pruning
minigit worktree prune -v
```

---

## 9. Submodules (`minigit submodule`)

Submodules allow keeping a MiniGit repository as a subdirectory of another MiniGit repository. The submodule has its own commit history, branches, and working tree, while the parent repository tracks the exact commit SHA of the submodule using Git-standard mode `160000` gitlinks.

### Add a Submodule (`minigit submodule add`)

Clones an external repository into a specified path inside your repository, records the mapping in `.minigitmodules` and `.minigit/config`, and stages the submodule gitlink into the parent index:

```bash
# Add a repository into libs/math
minigit submodule add https://github.com/example/math.git libs/math

# Specify a tracking branch and custom submodule name
minigit submodule add -b main --name math_lib https://github.com/example/math.git libs/math

# Stage and commit the new submodule gitlink and .minigitmodules
minigit commit -m "Add math submodule"
```

---

### Check Submodule Status (`minigit submodule status`)

Displays the status of all registered submodules. Each line begins with a status indicator:
- ` ` (space): Clean and in sync with the parent commit.
- `-` (minus): Submodule is uninitialized (working tree not populated).
- `+` (plus): Submodule working tree HEAD differs from the commit recorded in the parent index/tree.
- `U`: Unmerged merge conflict on the submodule entry.

```bash
minigit submodule status
```
*Output:*
```text
 a1b2c3d4e5f60718293a4b5c6d7e8f90123456789abcdef0123456789abcdef0 libs/math (main)
```

---

### Initialize and Update Submodules (`minigit submodule init` / `update`)

When cloning a repository with submodules, submodules start uninitialized. Use `init` and `update` to populate them:

```bash
# 1. Register submodules into local .minigit/config
minigit submodule init

# 2. Clone and checkout the recorded commits
minigit submodule update

# Or initialize and update in a single command:
minigit submodule update --init

# Recursively update nested submodules
minigit submodule update --init --recursive
```

---

### Run Commands in All Submodules (`minigit submodule foreach`)

Evaluates a command inside the working directory of each registered submodule. Useful for running builds, testing, or checking status across dependencies:

```bash
# Check status in every submodule
minigit submodule foreach minigit status

# Pull upstream changes across submodules
minigit submodule foreach minigit pull origin main
```

---

### Deinitialize Submodules (`minigit submodule deinit`)

Unregisters a submodule, removes its working directory to free disk space, while safely keeping its repository storage intact inside `.minigit/modules/`:

```bash
# Deinitialize a specific submodule
minigit submodule deinit libs/math

# Deinitialize all submodules
minigit submodule deinit --all

# Force deinitialization even if local modifications exist
minigit submodule deinit -f libs/math
```

---

### Synchronize Remote URLs (`minigit submodule sync`)

Updates local `.minigit/config` and internal submodule remote URLs if the URL in `.minigitmodules` was changed:

```bash
minigit submodule sync
```

---

### Inspect Submodule Commit Differences (`minigit submodule summary`)

Displays commit differences between the commit recorded in the parent repository and the current submodule HEAD:

```bash
minigit submodule summary
```

---

## 10. Binary Search Debugging (`minigit bisect`)

MiniGit includes a full DAG-aware binary search engine to isolate the commit that introduced a bug or behavioral change.

### Start a Bisection Session (`minigit bisect start`)

Initializes a bisect session, saving the current HEAD or branch name to `.minigit/BISECT_START`:

```bash
# Start an interactive bisection session
minigit bisect start

# Start with known bad and good bounds directly
minigit bisect start HEAD v1.0.0

# Start without checking out files (useful for external scripts/tools)
minigit bisect start --no-checkout
```

---

### Mark Commits as Bad, Good, or Skip (`minigit bisect bad` / `good` / `skip`)

Guide the bisection search by marking tested commits:

```bash
# Mark current commit (or specified SHA) as bad (has the bug)
minigit bisect bad
minigit bisect bad a1b2c3d

# Mark current commit (or specified SHA) as good (clean / bug not present)
minigit bisect good
minigit bisect good v1.0.0

# Mark current commit as untestable (e.g. broken build unrelated to bug)
minigit bisect skip
```

At each step, MiniGit automatically chooses the optimal midpoint in the DAG, reports the remaining steps, and checks out the candidate commit:
```text
Bisecting: 6 revisions left to test after this (roughly 2 steps)
[3e45b76] Refactor network socket buffers
```

---

### Automated Bisection Runner (`minigit bisect run`)

Automates the entire bisection process by running a test script at every midpoint:

```bash
# Run an automated test script or command
minigit bisect run ./test_regression.sh

# Run a test suite with arguments
minigit bisect run ctest --output-on-failure
```

MiniGit interprets the exit code of your script:
- `0`: Commit is good.
- `125`: Commit cannot be tested (triggers `skip`).
- `1` to `127` (except `125`): Commit is bad.

---

### Session History & Replay (`minigit bisect log` / `replay`)

MiniGit maintains a complete audit log of all bisection actions in `.minigit/BISECT_LOG`:

```bash
# View the command log of the current bisect session
minigit bisect log

# Save the log to a file for team sharing or archiving
minigit bisect log > bisect_session.log

# Replay an exported bisect log to restore exact state
minigit bisect replay bisect_session.log
```

---

### Custom Terms (`minigit bisect terms`)

When debugging changes that are not regressions (e.g. finding when a performance improvement was introduced), customize the terms:

```bash
# View current terms
minigit bisect terms

# Set custom terms when starting bisection
minigit bisect start --term-new fixed --term-old broken

# Or set custom terms explicitly
minigit bisect terms --term-bad broken --term-good fixed
```

---

### Finish and Clean Up (`minigit bisect reset`)

Terminates bisection, deletes all temporary bisect references and log files, and returns to the original branch checked out before bisection:

```bash
# Reset to the starting branch (recorded in BISECT_START)
minigit bisect reset

# Or reset and checkout a specific commit/branch directly
minigit bisect reset main
```

---

## 11. Remote Repositories & Synchronization

MiniGit supports both **local-filesystem repositories** and **Smart HTTP / HTTPS network remotes** (using Git Smart HTTP Transfer Protocol v1 over `libcurl`).

### Clone a Repository (`minigit clone`)

Clones an existing repository into a target directory, sets up `origin` tracking in `.minigit/config`, and checks out `HEAD`. Accepts both local filesystem paths and remote `http://` / `https://` URLs:

```bash
# Clone from local filesystem repository
minigit clone C:/projects/upstream downstream_repo
cd downstream_repo

# Clone over Smart HTTP / HTTPS
minigit clone https://github.com/example/sample-repo.git sample_repo
cd sample_repo

# Clone into a custom destination directory
minigit clone http://git.internal.lan/team/core.git custom_dest
```

---

### Manage Remotes (`minigit remote`)

Inspect and manage remote repository endpoints configured in `.minigit/config`:

```bash
# List remote names
minigit remote

# List remotes with URLs (verbose)
minigit remote -v

# Add a local filesystem remote
minigit remote add backup C:/backups/minigit_repo

# Add an HTTP / HTTPS remote
minigit remote add origin https://github.com/example/sample-repo.git

# Remove a remote
minigit remote remove backup
```

---

### Fetch Updates (`minigit fetch`)

Downloads missing commits, trees, and blobs from a local or HTTP/HTTPS remote and updates remote-tracking references under `.minigit/refs/remotes/<remote>/<branch>` without modifying local branches:

```bash
# Fetch from origin remote (local or HTTP/HTTPS)
minigit fetch origin

# Output indicates new branches or fast-forward commit ranges:
# From https://github.com/example/sample-repo.git
#    1a2b3c4..5d6e7f8  main -> origin/main
#  * [new branch]      feature-auth -> origin/feature-auth
```

---

### Push Commits (`minigit push`)

Transfers missing objects to the remote (using delta-compressed packfiles over Smart HTTP `/git-receive-pack`) and advances the remote branch reference using fast-forward verification:

```bash
# Push active branch to origin
minigit push origin main

# Push a specific topic branch
minigit push origin feature-oauth
```

> [!NOTE]
> MiniGit enforces canonical Git safety rules: non-fast-forward updates are rejected if the remote contains commits you do not possess locally. Run `minigit pull` or `minigit rebase` before re-pushing.

---

### Pull & Fast-Forward (`minigit pull`)

Fetches latest objects from the remote and merges or fast-forwards the tracking branch into the active local branch:

```bash
# Pull changes from default tracking remote and branch
minigit pull

# Pull specific branch from origin
minigit pull origin main
```

---

### Test Environment SSL Verification

For self-signed SSL certificates, internal staging servers, or automated test pipelines, disable SSL certificate validation using standard environment variables:

```bash
# Windows (PowerShell)
$env:GIT_SSL_NO_VERIFY="1"
.\minigit.exe clone https://self-signed.local/repo.git

# Linux / macOS (Bash)
GIT_SSL_NO_VERIFY=1 ./minigit clone https://self-signed.local/repo.git
```

---

## 12. Ignoring Files (`.minigitignore`)

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

## 13. Low-Level Plumbing Commands

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

### List Staged & Working Tree Files (`minigit ls-files`)

Inspects the staging area (`.minigit/index`) and compares tracked items against the working directory:

| Option | Description |
| :--- | :--- |
| `-c`, `--cached` | Show cached/tracked files (default). |
| `-s`, `--stage` | Show mode, SHA-256 hash, stage number (`0`), and path. |
| `-d`, `--deleted` | Show tracked files deleted in working tree. |
| `-m`, `--modified` | Show tracked files modified in working tree. |
| `-o`, `--others` | Show untracked files in working tree (respects `.minigitignore`). |
| `[<path>...]` | Filter output to specific file or directory prefix. |

```bash
# List all tracked files
minigit ls-files

# Show staged blob hashes and file modes
minigit ls-files -s

# Show untracked files
minigit ls-files -o

# Show modified or deleted files
minigit ls-files -m
minigit ls-files -d
```

---

### Inspect Tree Objects (`minigit ls-tree`)

Inspects the contents of a tree object referenced by a tree-ish (tree SHA, commit SHA, branch, tag, or `HEAD`):

| Option | Description |
| :--- | :--- |
| `-r` | Recurse into sub-trees. |
| `-d` | Show only tree entries (directories). |
| `-t` | Show tree entries even when recursing with `-r`. |
| `--name-only` | Show filenames/paths only. |
| `--object-only` | Show object SHA-256 hashes only. |
| `[<path>...]` | Filter output to specific paths. |

```bash
# Inspect root tree of HEAD
minigit ls-tree HEAD

# Inspect recursively
minigit ls-tree -r HEAD

# List only file paths
minigit ls-tree -r --name-only HEAD

# Inspect specific subtree or directory
minigit ls-tree HEAD src/
```

### Packfile Maintenance & Compaction (`minigit repack`)

Consolidates loose objects in `.minigit/objects/` into a single, delta-compressed binary packfile (`.pack`) and index (`.idx`).

```bash
# Repack loose objects into packfiles
minigit repack

# Repack and delete original loose objects (-d)
minigit repack -d

# Repack with custom delta window size (default: 10)
minigit repack -d -w 15
```

### Verify Packfiles (`minigit verify-pack`)

Validates CRC-32 object checksums and SHA-256 integrity of packfiles and indices.

```bash
# Verify packfile integrity
minigit verify-pack .minigit/objects/pack/pack-*.pack

# Verbose inspection displaying object types, sizes, offsets, and delta chains
minigit verify-pack -v .minigit/objects/pack/pack-*.pack
```

### Display Version Information (`minigit version`)

Prints the current MiniGit version string:

```bash
minigit version
# or standard flags
minigit --version
minigit -v
```

*Output:*
```text
minigit version 1.10.0
```

---

## 14. Checking for Updates & Self-Update (`minigit update`)

MiniGit includes an integrated self-update subsystem that queries official releases on GitHub, validates semantic version precedence, and automatically downloads and safely replaces the active executable in-place.

### 14.1 Check for Available Updates

To check if a newer release of MiniGit is available without applying updates:

```bash
minigit update --check
```

Example output if an update is available:
```text
A new version of minigit is available: v1.10.0 -> v1.11.0
Release: MiniGit v1.11.0
Published: 2026-09-23T12:00:00Z
Release URL: https://github.com/sagarkrjha/minigit/releases/tag/v1.11.0

Run 'minigit update' to upgrade.
```

If the installed version is already the newest release:
```text
minigit is already up to date (v1.10.0).
```

### 14.2 Download and Apply Self-Update

To automatically download the matching native binary for your platform (`minigit.exe` on Windows, `minigit-linux` on Linux, `minigit-macos` on macOS) and update the running binary in-place:

```bash
minigit update
```

Example output:
```text
Checking for latest release from sagarkrjha/minigit...
Found v1.11.0 (current: v1.10.0)
Downloading minigit.exe from https://github.com/sagarkrjha/minigit/releases/download/v1.11.0/minigit.exe...
Replacing executable at C:\Users\user\bin\minigit.exe...
Successfully updated minigit to v1.11.0!
Updated binary: C:\Users\user\bin\minigit.exe
```

Use `--force` or `-f` to reinstall the current release if desired:
```bash
minigit update --force
```

### 14.3 Terminal Update Notification Banner

When using MiniGit interactively, a lightweight non-intrusive update notification banner is automatically displayed after successful command completion when a newer release is detected:

```text
┌─────────────────────────────────────────────────────────────┐
│  A new version of minigit is available: v1.10.0 -> v1.11.0  │
│  Run 'minigit update' to update to the latest release       │
└─────────────────────────────────────────────────────────────┘
```

- **Smart Caching:** Release checks are cached in `.minigit/update_cache` (or `~/.minigit/update_cache`) for 24 hours to prevent network latency on daily commands.
- **Scripting & CI Safe:** Suppressed automatically in non-interactive environments, piped outputs, and when running plumbing commands.
- **Opt-out:** Disable update notifications anytime by setting `MINIGIT_NO_UPDATE_NOTIFIER=1`.

---

## 15. Command Summary & Cheat Sheet

| Command | Synopsis | Description |
| :--- | :--- | :--- |
| `version` | `minigit version` \| `--version` \| `-v` | Display MiniGit version information. |
| `update` | `minigit update [--check] [--force] [--repo <owner/repo>]` | Check for newer releases and self-update the MiniGit executable. |
| `install` | `minigit install [--system\|--user] [--dir <p>] [--no-path] [--no-context-menu] [-f] [--uninstall]` | Deploy Git-style layout, register PATH (<root>/cmd), Apps & Features, and context menu. |
| `init` | `minigit init` | Initialize a new repository or reinitialize an existing one. |
| `status` | `minigit status` | Report status across Working Tree, Index, and HEAD. |
| `clean` | `minigit clean [-f\|--force] [-n\|--dry-run] [-d] [-x] [<path>...]` | Remove untracked files and directories from working tree. |
| `ls-files` | `minigit ls-files [-s\|-c\|-d\|-m\|-o] [<path>...]` | Inspect index entries and working-tree status. |
| `ls-tree` | `minigit ls-tree [-d] [-r] [-t] [--name-only\|--object-only] <tree-ish> [<path>...]` | Inspect contents of a tree CAS object. |
| `add` | `minigit add (<file>\|<dir>\|.) ...` | Stage files, directories, or entire working tree into index. |
| `commit` | `minigit commit -m <msg> [--author <a>]` | Record staged snapshot into an immutable commit object. |
| `log` | `minigit log` | Traverse linear commit history backward from HEAD. |
| `show` | `minigit show [--stat\|--name-only] [<obj>]` | Inspect commit metadata with parent diff, tags, trees, or blobs. |
| `diff` | `minigit diff [--cached\|--staged] [<path>...]` | Show line-level unified diffs (unstaged or staged). |
| `branch` | `minigit branch [-d <name>] [<name>]` | List, create, or delete branches. |
| `switch` | `minigit switch [-c] <branch>` | Switch active branch, optionally creating it first with `-c`. |
| `checkout`| `minigit checkout <branch-or-sha>` | Restore working files and index to a branch or commit. |
| `worktree`| `minigit worktree [add\|list\|remove\|prune\|lock\|unlock\|move] [<args>]` | Manage multiple linked working trees sharing single CAS. |
| `submodule`| `minigit submodule [add\|status\|init\|update\|deinit\|summary\|foreach\|sync] [<args>]` | Manage nested repositories and mode 160000 gitlinks. |
| `bisect` | `minigit bisect [help\|start\|bad\|good\|new\|old\|skip\|reset\|terms\|log\|replay\|run] [<args>]` | Binary search debugging engine to pinpoint regression-introducing commits. |
| `tag` | `minigit tag [-a -m msg \| -d] [<name>]` | List, create (lightweight / annotated), or delete tags. |
| `reset` | `minigit reset [--soft\|--mixed\|--hard] <sha>`| Roll back HEAD, index, and/or working tree. |
| `merge` | `minigit merge <branch> [--author <a>]` | Merge a branch into HEAD with LCA 3-way merge engine. |
| `revert` | `minigit revert <commit> [--author <a>]` | Create a new commit inverting changes of a previous commit. |
| `cherry-pick` | `minigit cherry-pick [-n\|--no-commit] [--author <a>] [-m <p>] <c>` | Transplant changes from a commit onto current branch. |
| `rebase` | `minigit rebase [-i] [--onto <nb>] <up> \| --continue \| --abort \| --skip` | Replay commits linearly onto upstream base. |
| `stash` | `minigit stash [push\|list\|pop\|drop\|show]` | Shelve uncommitted modifications without committing. |
| `remote` | `minigit remote [add\|remove\|-v] [<args>]` | Inspect or manage remote repository aliases. |
| `clone` | `minigit clone <src> [<dest>]` | Clone a repository, set up tracking, and check out HEAD. |
| `fetch` | `minigit fetch [<remote>]` | Download objects and update remote-tracking references. |
| `push` | `minigit push [<remote> [<branch>]]` | Upload objects and advance remote branch references. |
| `pull` | `minigit pull [<remote> [<branch>]]` | Fetch and fast-forward the current branch with remote updates. |
| `repack` | `minigit repack [-a] [-d] [-w <n>]` | Consolidate loose objects into binary packfile with delta compression. |
| `verify-pack` | `minigit verify-pack [-v\|--verbose] <pack>...` | Validate cryptographic integrity and CRC-32 of packfiles. |
| `hash-object` | `minigit hash-object [-w] <file>` | Compute SHA-256 for a file; optionally persist as blob. |
| `write-tree` | `minigit write-tree` | Serialize current index entries into a tree object. |
| `cat-file` | `minigit cat-file (-t\|-s\|-p) <sha>` | Inspect object type, size, or pretty-print contents. |
