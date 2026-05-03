// main_test.cpp — entry point for the DrumMachine host test runner.
//
// Iterates the Meyers-singleton registry from test_macros.h, runs each case
// inside a try/catch wall, and prints `passed/failed/total` on the final line.
// Exit code is EXIT_SUCCESS iff failed == 0.

#include "test_macros.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    auto&        registry = ::drum_machine_test::Registry();
    const size_t total    = registry.size();
    size_t       passed   = 0;
    size_t       failed   = 0;

    for (const auto& tc : registry) {
        try {
            tc.fn();
            ++passed;
            std::cout << "[PASS] " << tc.name << "\n";
        } catch (const ::drum_machine_test::TestFailure& e) {
            ++failed;
            std::cout << "[FAIL] " << tc.name << ": " << e.what() << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << tc.name
                      << ": uncaught std::exception: " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "[FAIL] " << tc.name << ": uncaught unknown exception\n";
        }
    }

    std::cout << "passed=" << passed << " failed=" << failed
              << " total=" << total << "\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
