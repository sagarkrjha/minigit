import os
import sys
import subprocess
import json
from typing import List, Optional, Dict, Any

class MiniGitError(Exception):
    pass

class MiniGitClient:
    """
    Python SDK Client for MiniGit version control system.
    Communicates via native CLI subprocess or direct repository filesystem inspection.
    """
    def __init__(self, repo_path: str, binary_path: Optional[str] = None):
        self.repo_path = os.path.abspath(repo_path)
        self.binary_path = binary_path or self._resolve_binary()

    @staticmethod
    def _resolve_binary() -> str:
        # Check environment variable
        if "MINIGIT_BIN" in os.environ:
            return os.environ["MINIGIT_BIN"]
        # Check PATH
        path_bin = "minigit.exe" if sys.platform == "win32" else "minigit"
        # Try local build directories
        candidates = [
            path_bin,
            os.path.join("build", path_bin),
            os.path.join("bin", path_bin),
            os.path.join(os.path.dirname(__file__), "..", "..", "build", path_bin),
            os.path.join(os.path.dirname(__file__), "..", "..", "bin", path_bin),
        ]
        for c in candidates:
            if os.path.exists(c):
                return os.path.abspath(c)
        return path_bin

    def _run(self, *args: str) -> subprocess.CompletedProcess:
        cmd = [self.binary_path] + list(args)
        proc = subprocess.run(
            cmd,
            cwd=self.repo_path,
            capture_output=True,
            text=True
        )
        if proc.returncode != 0:
            raise MiniGitError(f"Command {' '.join(cmd)} failed (code {proc.returncode}): {proc.stderr.strip() or proc.stdout.strip()}")
        return proc

    @classmethod
    def init(cls, repo_path: str, binary_path: Optional[str] = None) -> "MiniGitClient":
        os.makedirs(repo_path, exist_ok=True)
        client = cls(repo_path, binary_path)
        client._run("init")
        return client

    @classmethod
    def open(cls, repo_path: str, binary_path: Optional[str] = None) -> "MiniGitClient":
        client = cls(repo_path, binary_path)
        if not os.path.exists(os.path.join(client.repo_path, ".minigit")):
            raise MiniGitError(f"Not a valid MiniGit repository: {repo_path}")
        return client

    def add(self, paths: List[str]) -> bool:
        self._run("add", *paths)
        return True

    def add_all(self) -> bool:
        self._run("add", ".")
        return True

    def commit(self, message: str, author: Optional[str] = None) -> str:
        args = ["commit", "-m", message]
        if author:
            args.extend(["--author", author])
        proc = self._run(*args)
        # Parse output: "[<short_sha>] <message>"
        output = proc.stdout.strip()
        return output

    def status(self) -> str:
        proc = self._run("status")
        return proc.stdout.strip()

    def list_branches(self) -> List[str]:
        proc = self._run("branch")
        branches = []
        for line in proc.stdout.strip().splitlines():
            clean = line.strip().lstrip("* ").strip()
            if clean and clean != "(no branches)":
                branches.append(clean)
        return branches

    def current_branch(self) -> str:
        head_path = os.path.join(self.repo_path, ".minigit", "HEAD")
        if os.path.exists(head_path):
            with open(head_path, "r", encoding="utf-8") as f:
                content = f.read().strip()
                if content.startswith("ref: refs/heads/"):
                    return content[len("ref: refs/heads/"):]
        return "HEAD (detached)"

    def create_branch(self, branch_name: str) -> bool:
        self._run("branch", branch_name)
        return True

    def switch_branch(self, branch_name: str) -> bool:
        self._run("switch", branch_name)
        return True

    def log(self) -> str:
        proc = self._run("log")
        return proc.stdout.strip()
