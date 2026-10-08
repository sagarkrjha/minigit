export class MiniGitError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'MiniGitError';
  }
}

export interface CommitResult {
  commitId: string;
  rawOutput: string;
}

export declare class MiniGitClient {
  readonly repoPath: string;
  readonly binaryPath: string;

  constructor(repoPath: string, binaryPath?: string | null);
  static init(repoPath: string, binaryPath?: string | null): MiniGitClient;
  static open(repoPath: string, binaryPath?: string | null): MiniGitClient;

  add(paths: string[]): boolean;
  addAll(): boolean;
  commit(message: string, author?: string | null): string;
  status(): string;
  listBranches(): string[];
  currentBranch(): string;
  createBranch(branchName: string): boolean;
  switchBranch(branchName: string): boolean;
  log(): string;
}
