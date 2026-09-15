#include "test_framework.h"
#include <iostream>

int main()
{
    const auto& tests = test::get_tests();
    std::cout << "========================================\n";
    std::cout << "Running " << tests.size() << " MiniGit automated tests...\n";
    std::cout << "========================================\n";

    size_t passed = 0;
    size_t failed = 0;

    for (const auto& t : tests)
    {
        std::cout << "[ RUN      ] " << t.suite << "." << t.name << "\n";
        try
        {
            t.fn();
            std::cout << "[       OK ] " << t.suite << "." << t.name << "\n";
            passed++;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[  FAILED  ] " << t.suite << "." << t.name << ": " << e.what() << "\n";
            failed++;
        }
        catch (...)
        {
            std::cerr << "[  FAILED  ] " << t.suite << "." << t.name << ": Unknown exception\n";
            failed++;
        }
    }

    std::cout << "========================================\n";
    std::cout << "Test Summary: " << passed << " passed, " << failed << " failed, " << tests.size() << " total\n";
    std::cout << "========================================\n";

    return (failed == 0) ? 0 : 1;
}
