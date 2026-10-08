use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, thiserror::Error)]
pub enum MiniGitError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("Not a MiniGit repository: {0}")]
    NotARepository(PathBuf),
    #[error("Command execution failed: {0}")]
    CommandFailed(String),
}

/// Client for interacting with a MiniGit repository via Rust.
#[derive(Debug, Clone)]
pub struct MiniGitClient {
    repo_path: PathBuf,
    binary_path: PathBuf,
}

impl MiniGitClient {
    /// Resolve minigit executable binary path from environment or defaults.
    pub fn resolve_binary() -> PathBuf {
        if let Ok(bin) = std::env::var("MINIGIT_BIN") {
            return PathBuf::from(bin);
        }

        let bin_name = if cfg!(windows) { "minigit.exe" } else { "minigit" };
        let candidates = [
            PathBuf::from(bin_name),
            PathBuf::from("build").join(bin_name),
            PathBuf::from("bin").join(bin_name),
        ];

        for c in &candidates {
            if c.exists() {
                if let Ok(abs) = fs::canonicalize(c) {
                    return abs;
                }
                return c.clone();
            }
        }

        PathBuf::from(bin_name)
    }

    /// Creates a client for an existing or target repository path.
    pub fn new<P: AsRef<Path>>(repo_path: P) -> Self {
        Self {
            repo_path: repo_path.as_ref().to_path_buf(),
            binary_path: Self::resolve_binary(),
        }
    }

    /// Sets custom path to the minigit executable.
    pub fn with_binary<P: AsRef<Path>>(mut self, binary_path: P) -> Self {
        self.binary_path = binary_path.as_ref().to_path_buf();
        self
    }

    /// Initialize a new MiniGit repository.
    pub fn init<P: AsRef<Path>>(repo_path: P) -> Result<Self, MiniGitError> {
        let path = repo_path.as_ref();
        fs::create_directories_all_safe(path)?;
        let client = Self::new(path);
        client.run(&["init"])?;
        Ok(client)
    }

    /// Open an existing MiniGit repository.
    pub fn open<P: AsRef<Path>>(repo_path: P) -> Result<Self, MiniGitError> {
        let client = Self::new(repo_path);
        if !client.repo_path.join(".minigit").exists() {
            return Err(MiniGitError::NotARepository(client.repo_path.clone()));
        }
        Ok(client)
    }

    fn run(&self, args: &[&str]) -> Result<String, MiniGitError> {
        let output = Command::new(&self.binary_path)
            .args(args)
            .current_dir(&self.repo_path)
            .output()?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            let stdout = String::from_utf8_lossy(&output.stdout);
            let msg = if !stderr.trim().is_empty() {
                stderr.trim().to_string()
            } else {
                stdout.trim().to_string()
            };
            return Err(MiniGitError::CommandFailed(msg));
        }

        Ok(String::from_utf8_lossy(&output.stdout).trim().to_string())
    }

    /// Stage specific files into the index.
    pub fn add(&self, paths: &[&str]) -> Result<(), MiniGitError> {
        let mut args = vec!["add"];
        args.extend_from_slice(paths);
        self.run(&args)?;
        Ok(())
    }

    /// Stage all modified and untracked files into the index.
    pub fn add_all(&self) -> Result<(), MiniGitError> {
        self.add(&["."])
    }

    /// Commit staged changes.
    pub fn commit(&self, message: &str, author: Option<&str>) -> Result<String, MiniGitError> {
        let mut args = vec!["commit", "-m", message];
        if let Some(auth) = author {
            args.extend_from_slice(&["--author", auth]);
        }
        self.run(&args)
    }

    /// Get working-tree status report.
    pub fn status(&self) -> Result<String, MiniGitError> {
        self.run(&["status"])
    }

    /// Get commit log output.
    pub fn log(&self) -> Result<String, MiniGitError> {
        self.run(&["log"])
    }

    /// Query current branch name or detached state.
    pub fn current_branch(&self) -> String {
        let head_path = self.repo_path.join(".minigit").join("HEAD");
        if let Ok(content) = fs::read_to_string(head_path) {
            let trimmed = content.trim();
            if let Some(branch) = trimmed.strip_prefix("ref: refs/heads/") {
                return branch.to_string();
            }
        }
        "HEAD (detached)".to_string()
    }

    /// Create a new branch pointing at current HEAD.
    pub fn create_branch(&self, branch_name: &str) -> Result<(), MiniGitError> {
        self.run(&["branch", branch_name])?;
        Ok(())
    }

    /// Switch HEAD to an existing branch.
    pub fn switch_branch(&self, branch_name: &str) -> Result<(), MiniGitError> {
        self.run(&["switch", branch_name])?;
        Ok(())
    }
}

fn create_directories_all_safe(path: &Path) -> std::io::Result<()> {
    if !path.exists() {
        fs::create_dir_all(path)?;
    }
    Ok(())
}
