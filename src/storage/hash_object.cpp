#include "hash_object.h"

#include "core/file.h"
#include "blob.h"
#include "object_database.h"
#include "repository/repository.h"

#include <iostream>
#include <stdexcept>

void hash_object(const std::filesystem::path &path, bool write)
{
    // Read the file contents.
    std::string content;
    try
    {
        content = read_file(path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return;
    }

    // Build a blob object and compute its id.
    Blob blob(std::move(content));

    if (write)
    {
        // Discover the repository root (works from nested directories too).
        Repository repo = Repository::discover(std::filesystem::current_path());
        ObjectDatabase db(repo.git_dir() / "objects");

        try
        {
            db.write(blob.id(), blob.serialized());
        }
        catch (const std::exception &e)
        {
            std::cerr << "error: " << e.what() << '\n';
            return;
        }
    }

    std::cout << blob.id() << '\n';
}