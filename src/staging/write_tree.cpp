#include "write_tree.h"

#include "index.h"
#include "storage/object_database.h"
#include "storage/tree.h"
#include "repository/repository.h"
#include "submodule/submodule_config.h"

#include <iostream>
#include <stdexcept>

void write_tree()
{
    Repository repo = [&]() -> Repository {
        try {
            return Repository::discover(std::filesystem::current_path());
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            std::exit(1);
        }
    }();

    Index index(repo.git_dir() / "index");

    if (index.entries().empty())
    {
        std::cerr << "error: nothing to write — index is empty\n";
        std::exit(1);
    }

    // Build tree entries from the index (flat index → flat tree for now).
    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(index.entries().size());

    for (const auto &[path, blob_id] : index.entries())
    {
        const std::string mode = SubmoduleConfig::is_submodule_path(repo.root(), path) ? "160000" : "100644";
        tree_entries.push_back({mode, path, blob_id});
    }

    Tree tree(std::move(tree_entries));

    ObjectDatabase db(repo.git_dir() / "objects");
    try
    {
        db.write(tree.id(), tree.serialized());
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        std::exit(1);
    }

    std::cout << tree.id() << '\n';
}
