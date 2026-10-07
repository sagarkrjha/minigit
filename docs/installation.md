# MiniGit Installation & Build Guide

This document contains complete instructions for building MiniGit from source and using the built-in permission-based installer across Windows, Linux, and macOS.

> 📖 For general CLI usage, see [USAGE.md](../USAGE.md). For system architecture and design specifications, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Table of Contents

- [1. Prerequisites](#1-prerequisites)
- [2. Pre-Compiled Binaries](#2-pre-compiled-binaries)
- [3. Building from Source](#3-building-from-source)
  - [Windows (PowerShell / MSVC / Ninja)](#windows-powershell--msvc--ninja)
  - [Linux (Ubuntu / Debian / Fedora)](#linux-ubuntu--debian--fedora)
  - [macOS (Homebrew / Apple Silicon)](#macos-homebrew--apple-silicon)
- [4. Permission-Based Installation (`minigit install`)](#4-permission-based-installation-minigit-install)
  - [Directory Structure](#directory-structure)
  - [Installation Scopes](#installation-scopes)
  - [Per-User Installation](#per-user-installation)
  - [System-Wide Installation](#system-wide-installation)
  - [Advanced Installer Options](#advanced-installer-options)
  - [Uninstallation](#uninstallation)
- [5. Automated Self-Update (`minigit update`)](#5-automated-self-update-minigit-update)

---

## 1. Prerequisites

MiniGit requires a standard modern C++20 toolchain along with cryptographic and compression libraries:

| Dependency | Minimum Version | Description |
| :--- | :--- | :--- |
| **C++ Compiler** | C++20 compliant | GCC 11+, Clang 13+, or MSVC 2019/2022 (VS 16.10+) |
| **CMake** | 3.20+ | Cross-platform build generator |
| **OpenSSL** | 3.0+ | Cryptographic hashing (`OpenSSL::Crypto` with SHA-256 EVP interface) |
| **zlib** | 1.2.11+ | Lossless object compression (`ZLIB::ZLIB`) |
| **libcurl** | 7.68+ | HTTP/HTTPS network client for Smart HTTP transfers (`CURL::libcurl`) |

---

## 2. Pre-Compiled Binaries

Pre-compiled native standalone binaries are built and tested on every push:

- **Windows (x86_64):** [`minigit.exe`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit.exe)
- **Linux (x86_64):** [`minigit-linux`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-linux)
- **macOS (arm64):** [`minigit-macos`](https://github.com/sagarkrjha/minigit/releases/latest/download/minigit-macos)

---

## 3. Building from Source

### Windows (PowerShell / MSVC / Ninja)

#### 1. Clone the repository
```powershell
git clone https://github.com/sagarkrjha/minigit.git
cd minigit
```

#### 2. Install dependencies via vcpkg
Ensure `vcpkg` is installed and the `VCPKG_ROOT` environment variable is defined:
```powershell
vcpkg install openssl zlib curl:x64-windows
```

#### 3. Configure and build
```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

The resulting binaries are located in:
- CLI executable: `.\build\minigit.exe`
- Test suite: `.\build\minigit_tests.exe`
- Benchmarks: `.\build\minigit_benchmarks.exe`

---

### Linux (Ubuntu / Debian / Fedora)

#### 1. Install dependencies
```bash
# Ubuntu / Debian
sudo apt-get update && sudo apt-get install -y build-essential cmake libssl-dev zlib1g-dev libcurl4-openssl-dev

# Fedora
sudo dnf install -y gcc-c++ cmake openssl-devel zlib-devel libcurl-devel
```

#### 2. Configure and build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Executable output: `./build/minigit`.

---

### macOS (Homebrew / Apple Silicon)

#### 1. Install dependencies
```bash
brew install cmake openssl zlib curl
```

#### 2. Configure and build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

Executable output: `./build/minigit`.

---

## 4. Permission-Based Installation (`minigit install`)

MiniGit includes an integrated installer (`minigit install`) designed to match standard packaging conventions (such as Git for Windows). It organizes installation roots, configures system or user environment `PATH`, registers with OS package managers, and adds optional file explorer context menus.

### Directory Structure

```text
<InstallRoot>/
├── cmd/
│   └── minigit.exe        ← Directory added to PATH (prevents tool collisions)
├── bin/
│   └── minigit.exe        ← Core native binary
└── etc/
    ├── minigitconfig      ← Default system configuration ([core] autocrlf = true)
    └── templates/         ← Default repository template files
```

### Installation Scopes

| Scope | Windows Destination | Linux / macOS Destination | Elevation Required | PATH Target |
| :--- | :--- | :--- | :--- | :--- |
| **User** (`--user`) | `%LOCALAPPDATA%\Programs\MiniGit` | `~/.local` | No (Standard user) | User `PATH` (`<root>\cmd`) |
| **System** (`--system`) | `%ProgramFiles%\MiniGit` | `/usr/local` | Yes (Administrator / root) | System `PATH` (`<root>\cmd`) |

### Per-User Installation

Installs MiniGit for the current user without requiring elevation:

```powershell
# Windows
.\minigit.exe install --user
```
```bash
# Linux / macOS
./minigit-linux install --user
```

*On Windows, registers `<InstallRoot>\cmd` in user `PATH` and broadcasts `WM_SETTINGCHANGE` so existing and new terminal sessions immediately recognize `minigit`.*

### System-Wide Installation

Installs MiniGit for all users on the host:

```powershell
# Windows
.\minigit.exe install --system
```
```bash
# Linux / macOS
sudo ./minigit-linux install --system
```

> **Automated Windows UAC Escalation:** If `install --system` is executed from a non-elevated prompt, MiniGit automatically requests Administrator elevation via the Windows UAC consent dialog (`runas`). Once approved, the elevated worker process completes installation and updates the machine environment `PATH`.

### Advanced Installer Options

- **Custom Directory:** `--dir <path>` installs to a user-defined target path.
- **Skip PATH Modification:** `--no-path` installs binaries without modifying environment variables.
- **Skip Context Menu (Windows):** `--no-context-menu` avoids registering the "Open MiniGit Prompt Here" right-click action in Windows Explorer.
- **Force Overwrite:** `-f` or `--force` overwrites existing files.

### Uninstallation

To cleanly remove installed binaries, remove registry entries, restore environment `PATH`, and unregister shell context menus:

```powershell
minigit install --uninstall
```

On Windows, MiniGit also registers in **Installed Apps (Add or Remove Programs)** for standard OS uninstallation.

---

## 5. Automated Self-Update (`minigit update`)

MiniGit includes a self-updating mechanism that interacts with GitHub Releases:

```bash
# Check if a newer version is available without downloading
minigit update --check

# Download and apply update in-place
minigit update

# Force reinstall or update to latest release
minigit update --force
```
