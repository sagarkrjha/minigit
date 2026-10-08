const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

class MiniGitError extends Error {
  constructor(message) {
    super(message);
    this.name = 'MiniGitError';
  }
}

class MiniGitClient {
  constructor(repoPath, binaryPath = null) {
    this.repoPath = path.resolve(repoPath);
    this.binaryPath = binaryPath || MiniGitClient.resolveBinary();
  }

  static resolveBinary() {
    if (process.env.MINIGIT_BIN) {
      return process.env.MINIGIT_BIN;
    }
    const binName = process.platform === 'win32' ? 'minigit.exe' : 'minigit';
    const candidates = [
      binName,
      path.join('build', binName),
      path.join('bin', binName),
      path.join(__dirname, '..', '..', 'build', binName),
      path.join(__dirname, '..', '..', 'bin', binName)
    ];
    for (const c of candidates) {
      if (fs.existsSync(c)) {
        return path.resolve(c);
      }
    }
    return binName;
  }

  _run(args) {
    const cmd = `"${this.binaryPath}" ${args.join(' ')}`;
    try {
      return execSync(cmd, { cwd: this.repoPath, encoding: 'utf8' }).trim();
    } catch (err) {
      throw new MiniGitError(`MiniGit command failed: ${err.message}`);
    }
  }

  static init(repoPath, binaryPath = null) {
    fs.mkdirSync(repoPath, { recursive: true });
    const client = new MiniGitClient(repoPath, binaryPath);
    client._run(['init']);
    return client;
  }

  static open(repoPath, binaryPath = null) {
    const client = new MiniGitClient(repoPath, binaryPath);
    if (!fs.existsSync(path.join(client.repoPath, '.minigit'))) {
      throw new MiniGitError(`Not a valid MiniGit repository: ${repoPath}`);
    }
    return client;
  }

  add(paths) {
    this._run(['add', ...paths]);
    return true;
  }

  addAll() {
    this._run(['add', '.']);
    return true;
  }

  commit(message, author = null) {
    const args = ['commit', '-m', `"${message}"`];
    if (author) {
      args.push('--author', `"${author}"`);
    }
    return this._run(args);
  }

  status() {
    return this._run(['status']);
  }

  listBranches() {
    const output = this._run(['branch']);
    return output
      .split('\n')
      .map(line => line.replace(/^\*\s*/, '').trim())
      .filter(line => line && line !== '(no branches)');
  }

  currentBranch() {
    const headPath = path.join(this.repoPath, '.minigit', 'HEAD');
    if (fs.existsSync(headPath)) {
      const content = fs.readFileSync(headPath, 'utf8').trim();
      if (content.startsWith('ref: refs/heads/')) {
        return content.replace('ref: refs/heads/', '');
      }
    }
    return 'HEAD (detached)';
  }

  createBranch(branchName) {
    this._run(['branch', branchName]);
    return true;
  }

  switchBranch(branchName) {
    this._run(['switch', branchName]);
    return true;
  }

  log() {
    return this._run(['log']);
  }
}

module.exports = {
  MiniGitClient,
  MiniGitError
};
