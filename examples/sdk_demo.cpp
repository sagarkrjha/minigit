#if __has_include(<minigit/minigit_sdk.h>)
#include <minigit/minigit_sdk.h>
#elif __has_include("sdk/minigit_sdk.h")
#include "sdk/minigit_sdk.h"
#else
#include "minigit_sdk.h"
#endif

#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    std::cout << "========================================\n";
    std::cout << "MiniGit SDK Demo Application in Docker\n";
    std::cout << "========================================\n";

    fs::path repo_path = "/data/sdk-demo-repo";
    if (fs::exists(repo_path)) {
        fs::remove_all(repo_path);
    }

    std::cout << "[1] Initializing repository at " << repo_path << "...\n";
    auto client = minigit::sdk::MiniGitClient::init(repo_path);
    if (!client.is_valid()) {
        std::cerr << "Failed to initialize repository!\n";
        return 1;
    }

    std::cout << "[2] Creating sample source files...\n";
    std::ofstream file1(repo_path / "hello.cpp");
    file1 << "#include <iostream>\nint main() { std::cout << \"Hello from SDK!\\n\"; }\n";
    file1.close();

    std::ofstream file2(repo_path / "README.md");
    file2 << "# SDK Demo Repo\nCreated programmatically using libminigit_sdk.\n";
    file2.close();

    std::cout << "[3] Staging all files...\n";
    client.add_all();

    std::cout << "[4] Creating commit via SDK...\n";
    std::string cid = client.commit("Initial commit created with MiniGit SDK", "Docker Demo <demo@minigit.internal>");
    std::cout << "Created commit: " << cid << "\n";

    std::cout << "[5] Checking status...\n";
    auto st = client.status();
    std::cout << "Current branch: " << st.current_branch << "\n";
    std::cout << "Is clean: " << (st.is_clean ? "yes" : "no") << "\n";

    std::cout << "[6] Syncing backup to local object storage target (/data/backups)...\n";
    bool synced = client.sync_to_cloud("/data/backups", "sdk-demo");
    std::cout << "Cloud/local sync status: " << (synced ? "SUCCESS" : "FAILED") << "\n";

    std::cout << "[7] Reading commit history...\n";
    auto history = client.log(5);
    for (const auto& c : history) {
        std::cout << " - Commit: " << c.commit_id.substr(0, 8) << " | " << c.author << " | " << c.message << "\n";
    }

    std::cout << "========================================\n";
    std::cout << "Demo completed successfully!\n";
    std::cout << "========================================\n";

    return 0;
}
