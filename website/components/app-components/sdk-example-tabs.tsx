"use client";

import { CodeTabs } from "@/components/ui/code-tabs";

const SDK_TABS = [
  {
    label: "C++20",
    language: "cpp",
    code: `#include <minigit/minigit_sdk.h>
#include <iostream>

int main() {
    // 1. Initialize or open repository
    auto client = minigit::sdk::MiniGitClient::init("./my_repo");

    // 2. Stage files
    client.add({"file.txt"});

    // 3. Create commit
    std::string commit_id = client.commit("Initial commit via SDK", "Dev <dev@example.com>");
    std::cout << "Created commit: " << commit_id << "\\n";

    // 4. Branching
    client.create_branch("feature");
    client.switch_branch("feature");

    // 5. Query status and history
    auto status = client.status();
    std::cout << "Branch: " << status.current_branch << "\\n";

    auto logs = client.log(10);
    for (const auto& c : logs) {
        std::cout << c.commit_id << " - " << c.message << "\\n";
    }

    // 6. Cloud sync / backup
    client.sync_to_cloud("/data/backup_store", "my_repo");
    return 0;
}`,
  },
  {
    label: "Rust",
    language: "rust",
    code: `use minigit_sdk::MiniGitClient;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Initialize repository
    let client = MiniGitClient::init("./my_repo")?;
    
    // Stage all files and record snapshot
    client.add_all()?;
    let commit_id = client.commit(
        "Initial commit via Rust SDK",
        Some("Rustacean <rust@minigit.internal>")
    )?;
    
    println!("Committed: {}", commit_id);
    Ok(())
}`,
  },
  {
    label: "Python",
    language: "python",
    code: `from minigit_sdk import MiniGitClient

# Initialize repository
client = MiniGitClient.init("./my_repo")

# Stage and commit
client.add_all()
commit_id = client.commit("Initial commit via Python SDK")
print(f"Committed: {commit_id}")

# Branching
client.create_branch("feature/auth")
client.switch_branch("feature/auth")`,
  },
  {
    label: "TS / JS",
    language: "typescript",
    code: `import { MiniGitClient } from './sdks/js';

// Initialize repository
const client = MiniGitClient.init('./my_repo');

// Stage and commit
client.addAll();
const commitId = client.commit('Initial commit via JS SDK');
console.log(\`Committed: \${commitId}\`);

// Inspect status
const status = client.status();
console.log(\`Current branch: \${status.currentBranch}\`);`,
  },
  {
    label: "Go",
    language: "go",
    code: `package main

import (
    "fmt"
    "sdks/go/minigit"
)

func main() {
    client, _ := minigit.Init("./my_repo")
    client.AddAll()
    cid, _ := client.Commit("Initial commit via Go SDK")
    fmt.Println("Committed:", cid)
}`,
  },
  {
    label: "Java",
    language: "java",
    code: `import io.minigit.sdk.MiniGitClient;
import java.nio.file.Path;

public class App {
    public static void main(String[] args) throws Exception {
        MiniGitClient client = MiniGitClient.init(Path.of("./my_repo"));
        client.addAll();
        String cid = client.commit("Initial commit via Java SDK", "Dev <dev@minigit.internal>");
        System.out.println("Committed: " + cid);
    }
}`,
  },
  {
    label: "C (C99 FFI)",
    language: "c",
    code: `#include <minigit/minigit_sdk.h>
#include <stdio.h>

int main() {
    minigit_handle_t* repo = minigit_init("./my_repo");
    if (!repo) return 1;

    minigit_add(repo, "file.txt");
    const char* cid = minigit_commit(repo, "Initial C commit", "Dev <dev@example.com>");
    printf("Commit SHA: %s\\n", cid);

    minigit_free(repo);
    return 0;
}`,
  },
];

export function SdkExampleTabs() {
  return <CodeTabs tabs={SDK_TABS} />;
}
