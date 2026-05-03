// test_macros.h — header-only test framework for DrumMachine host tests.
//
// Per SPECIFICATION.md §Test Harness:
//   - EXPECT_* macros throw a TestFailure on assertion failure carrying
//     file, line, and a stringified message.
//   - TEST_CASE(name) self-registers into a Meyers-singleton registry so the
//     registry survives multi-TU linking under -O2.
//   - main_test.cpp iterates the registry and prints `passed/failed/total`.
//
// This file is intentionally header-only; see task 01 brief, "Notes / risks".

#pragma once

#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace drum_machine_test {

// Exception type thrown by EXPECT_* on failure.
struct TestFailure : public std::exception {
    std::string file;
    int         line;
    std::string message;
    std::string what_buffer;

    TestFailure(std::string f, int l, std::string m)
        : file(std::move(f)), line(l), message(std::move(m)) {
        std::ostringstream oss;
        oss << file << ":" << line << ": " << message;
        what_buffer = oss.str();
    }

    const char* what() const noexcept override { return what_buffer.c_str(); }
};

// One registered test case.
struct TestCase {
    std::string           name;
    std::function<void()> fn;
};

// Meyers-singleton registry. Returning by reference to a function-local static
// guarantees initialization-on-first-use, so TEST_CASE self-registrations
// during static init cannot race with the registry's own construction.
inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

// Helper used by TEST_CASE to register at static-init time.
struct AutoRegister {
    AutoRegister(const char* name, std::function<void()> fn) {
        Registry().push_back(TestCase{std::string(name), std::move(fn)});
    }
};

// Throw helper — marked [[noreturn]] so callers don't need to fall through.
[[noreturn]] inline void ThrowFailure(const char* file, int line,
                                      const std::string& msg) {
    throw TestFailure(file, line, msg);
}

}  // namespace drum_machine_test

// ---- Token-pasting helpers for unique identifiers in TEST_CASE ----
#define DM_TEST_CONCAT_INNER(a, b) a##b
#define DM_TEST_CONCAT(a, b) DM_TEST_CONCAT_INNER(a, b)

// TEST_CASE("name") { ... }  — defines a function and registers it.
#define TEST_CASE(name)                                                       \
    static void DM_TEST_CONCAT(dm_test_fn_, __LINE__)();                      \
    static ::drum_machine_test::AutoRegister DM_TEST_CONCAT(dm_test_reg_,     \
                                                            __LINE__)(       \
        name, &DM_TEST_CONCAT(dm_test_fn_, __LINE__));                        \
    static void DM_TEST_CONCAT(dm_test_fn_, __LINE__)()

// ---- Assertion macros ----
#define EXPECT_TRUE(cond)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_TRUE(" #cond ") failed";                       \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_FALSE(cond)                                                    \
    do {                                                                      \
        if ((cond)) {                                                         \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_FALSE(" #cond ") failed";                      \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        const auto _dm_a = (a);                                               \
        const auto _dm_b = (b);                                               \
        if (!(_dm_a == _dm_b)) {                                              \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_EQ(" #a ", " #b ") failed: " << _dm_a          \
                    << " != " << _dm_b;                                       \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_NEAR(a, b, eps)                                                \
    do {                                                                      \
        const double _dm_a   = static_cast<double>(a);                        \
        const double _dm_b   = static_cast<double>(b);                        \
        const double _dm_eps = static_cast<double>(eps);                      \
        const double _dm_d   = _dm_a - _dm_b;                                 \
        const double _dm_abs = _dm_d < 0 ? -_dm_d : _dm_d;                    \
        if (!(_dm_abs <= _dm_eps)) {                                          \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_NEAR(" #a ", " #b ", " #eps ") failed: |"      \
                    << _dm_a << " - " << _dm_b << "| = " << _dm_abs           \
                    << " > " << _dm_eps;                                      \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_LT(a, b)                                                       \
    do {                                                                      \
        const auto _dm_a = (a);                                               \
        const auto _dm_b = (b);                                               \
        if (!(_dm_a < _dm_b)) {                                               \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_LT(" #a ", " #b ") failed: " << _dm_a          \
                    << " >= " << _dm_b;                                       \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_LE(a, b)                                                       \
    do {                                                                      \
        const auto _dm_a = (a);                                               \
        const auto _dm_b = (b);                                               \
        if (!(_dm_a <= _dm_b)) {                                              \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_LE(" #a ", " #b ") failed: " << _dm_a          \
                    << " > " << _dm_b;                                        \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_GT(a, b)                                                       \
    do {                                                                      \
        const auto _dm_a = (a);                                               \
        const auto _dm_b = (b);                                               \
        if (!(_dm_a > _dm_b)) {                                               \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_GT(" #a ", " #b ") failed: " << _dm_a          \
                    << " <= " << _dm_b;                                       \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

#define EXPECT_GE(a, b)                                                       \
    do {                                                                      \
        const auto _dm_a = (a);                                               \
        const auto _dm_b = (b);                                               \
        if (!(_dm_a >= _dm_b)) {                                              \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_GE(" #a ", " #b ") failed: " << _dm_a          \
                    << " < " << _dm_b;                                        \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)
