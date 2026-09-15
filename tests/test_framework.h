#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

namespace test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline bool register_test(const std::string& suite, const std::string& name, std::function<void()> fn) {
    get_tests().push_back({suite, name, std::move(fn)});
    return true;
}

#define TEST_CASE(suite, name) \
    static void test_##suite##_##name(); \
    static const bool reg_##suite##_##name = test::register_test(#suite, #name, test_##suite##_##name); \
    static void test_##suite##_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #expr + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (false)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (false)

#define ASSERT_NE(a, b) \
    do { \
        if (!((a) != (b))) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " != " + #b + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (false)

} // namespace test
