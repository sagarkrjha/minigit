package minigit

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type MiniGitClient struct {
	RepoPath   string
	BinaryPath string
}

func resolveBinary() string {
	if bin := os.Getenv("MINIGIT_BIN"); bin != "" {
		return bin
	}
	binName := "minigit"
	candidates := []string{
		binName,
		binName + ".exe",
		filepath.Join("build", binName),
		filepath.Join("build", binName+".exe"),
		filepath.Join("bin", binName),
		filepath.Join("bin", binName+".exe"),
	}
	for _, c := range candidates {
		if _, err := os.Stat(c); err == nil {
			abs, err := filepath.Abs(c)
			if err == nil {
				return abs
			}
			return c
		}
	}
	return binName
}

func NewClient(repoPath string, binaryPath ...string) *MiniGitClient {
	bin := resolveBinary()
	if len(binaryPath) > 0 && binaryPath[0] != "" {
		bin = binaryPath[0]
	}
	abs, err := filepath.Abs(repoPath)
	if err != nil {
		abs = repoPath
	}
	return &MiniGitClient{
		RepoPath:   abs,
		BinaryPath: bin,
	}
}

func Init(repoPath string, binaryPath ...string) (*MiniGitClient, error) {
	if err := os.MkdirAll(repoPath, 0755); err != nil {
		return nil, err
	}
	client := NewClient(repoPath, binaryPath...)
	_, err := client.run("init")
	if err != nil {
		return nil, err
	}
	return client, nil
}

func Open(repoPath string, binaryPath ...string) (*MiniGitClient, error) {
	client := NewClient(repoPath, binaryPath...)
	gitDir := filepath.Join(client.RepoPath, ".minigit")
	if _, err := os.Stat(gitDir); os.IsNotExist(err) {
		return nil, fmt.Errorf("not a valid MiniGit repository: %s", repoPath)
	}
	return client, nil
}

func (c *MiniGitClient) run(args ...string) (string, error) {
	cmd := exec.Command(c.BinaryPath, args...)
	cmd.Dir = c.RepoPath

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()
	if err != nil {
		return "", fmt.Errorf("minigit command failed (%v): %s %s", err, stderr.String(), stdout.String())
	}
	return strings.TrimSpace(stdout.String()), nil
}

func (c *MiniGitClient) Add(paths ...string) error {
	args := append([]string{"add"}, paths...)
	_, err := c.run(args...)
	return err
}

func (c *MiniGitClient) AddAll() error {
	return c.Add(".")
}

func (c *MiniGitClient) Commit(message string, author ...string) (string, error) {
	args := []string{"commit", "-m", message}
	if len(author) > 0 && author[0] != "" {
		args = append(args, "--author", author[0])
	}
	return c.run(args...)
}

func (c *MiniGitClient) Status() (string, error) {
	return c.run("status")
}

func (c *MiniGitClient) Log() (string, error) {
	return c.run("log")
}

func (c *MiniGitClient) CurrentBranch() string {
	headPath := filepath.Join(c.RepoPath, ".minigit", "HEAD")
	data, err := os.ReadFile(headPath)
	if err == nil {
		content := strings.TrimSpace(string(data))
		if strings.HasPrefix(content, "ref: refs/heads/") {
			return strings.TrimPrefix(content, "ref: refs/heads/")
		}
	}
	return "HEAD (detached)"
}

func (c *MiniGitClient) CreateBranch(branchName string) error {
	_, err := c.run("branch", branchName)
	return err
}

func (c *MiniGitClient) SwitchBranch(branchName string) error {
	_, err := c.run("switch", branchName)
	return err
}
