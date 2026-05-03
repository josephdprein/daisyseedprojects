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

// ---- Audio / parameter assertion helpers (per spec §Test Harness) ----------
// "Peak" everywhere in the test harness is max-abs-sample, never RMS. These
// helpers operate on any container `buf` whose `size()` returns a count and
// whose `operator[]` yields a `float`-compatible sample (typical concrete
// types: std::vector<float>, std::array<float, N>).

namespace drum_machine_test {

// Inline so the helpers stay header-only. Returns the maximum |buf[i]|.
template <typename Buf>
inline double BufferPeak(const Buf& buf) {
    double peak = 0.0;
    const std::size_t n = buf.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double v = static_cast<double>(buf[i]);
        const double a = v < 0.0 ? -v : v;
        if (a > peak) peak = a;
    }
    return peak;
}

// Index of the first sample whose |buf[i]| exceeds `threshold`. Returns
// buf.size() if no sample crosses — callers compare against an upper bound.
template <typename Buf>
inline std::size_t FirstTransientIndex(const Buf& buf, double threshold) {
    const std::size_t n = buf.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double v = static_cast<double>(buf[i]);
        const double a = v < 0.0 ? -v : v;
        if (a > threshold) return i;
    }
    return n;
}

}  // namespace drum_machine_test

// Audible: peak ≥ 0.05 (per spec §Mixer "test_instruments asserts each voice
// in isolation has peak ≥ 0.05").
#define EXPECT_AUDIO_NOT_SILENT(buf)                                          \
    do {                                                                      \
        const double _dm_peak = ::drum_machine_test::BufferPeak(buf);         \
        if (!(_dm_peak >= 0.05)) {                                            \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_AUDIO_NOT_SILENT(" #buf ") failed: peak = "    \
                    << _dm_peak << " < 0.05";                                 \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

// Bounded: peak ≤ max. Used per-voice (max=1.0) and at the mixer (max=0.95).
#define EXPECT_AUDIO_PEAK_LE(buf, max)                                        \
    do {                                                                      \
        const double _dm_peak = ::drum_machine_test::BufferPeak(buf);         \
        const double _dm_max  = static_cast<double>(max);                     \
        if (!(_dm_peak <= _dm_max)) {                                         \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_AUDIO_PEAK_LE(" #buf ", " #max                 \
                    << ") failed: peak = " << _dm_peak << " > " << _dm_max;   \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

// First detectable transient occurs at index ≤ sampleIdx. Threshold is fixed
// at 0.01 (-40 dBFS) — well above DaisySP's idle DC noise floor but below the
// 0.05 audibility line, so a voice that just barely passes EXPECT_AUDIO_NOT_
// SILENT can still tell us *when* it first made a sample.
#define EXPECT_FIRST_TRANSIENT_WITHIN(buf, sampleIdx)                         \
    do {                                                                      \
        constexpr double _dm_threshold = 0.01;                                \
        const std::size_t _dm_first =                                         \
            ::drum_machine_test::FirstTransientIndex((buf), _dm_threshold);   \
        const std::size_t _dm_idx = static_cast<std::size_t>(sampleIdx);      \
        if (!(_dm_first <= _dm_idx)) {                                        \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_FIRST_TRANSIENT_WITHIN(" #buf ", " #sampleIdx  \
                    << ") failed: first transient at index " << _dm_first     \
                    << " > " << _dm_idx                                       \
                    << " (threshold=" << _dm_threshold << ")";                \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)

// std::array<float, N> element-wise equality with a small tolerance. Used to
// confirm Randomize() with depth=0 leaves a parameter snapshot untouched.
#define EXPECT_PARAMS_UNCHANGED(a, b)                                         \
    do {                                                                      \
        const auto& _dm_pa = (a);                                             \
        const auto& _dm_pb = (b);                                             \
        if (_dm_pa.size() != _dm_pb.size()) {                                 \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_PARAMS_UNCHANGED(" #a ", " #b                  \
                    << ") failed: size mismatch " << _dm_pa.size()            \
                    << " vs " << _dm_pb.size();                               \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
        constexpr double _dm_eps = 1e-5;                                      \
        for (std::size_t _dm_i = 0; _dm_i < _dm_pa.size(); ++_dm_i) {         \
            const double _dm_d = static_cast<double>(_dm_pa[_dm_i])           \
                               - static_cast<double>(_dm_pb[_dm_i]);          \
            const double _dm_abs = _dm_d < 0 ? -_dm_d : _dm_d;                \
            if (!(_dm_abs <= _dm_eps)) {                                      \
                std::ostringstream _dm_oss;                                   \
                _dm_oss << "EXPECT_PARAMS_UNCHANGED(" #a ", " #b              \
                        << ") failed at [" << _dm_i << "]: "                  \
                        << static_cast<double>(_dm_pa[_dm_i]) << " vs "       \
                        << static_cast<double>(_dm_pb[_dm_i]);                \
                ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,         \
                                                  _dm_oss.str());             \
            }                                                                 \
        }                                                                     \
    } while (0)

// At least one element of `a` differs from the matching element of `b` by
// more than `eps_relative * max(|a|+|b|, 1.0)`. Used to confirm Randomize()
// with depth>0 actually moves parameters.
#define EXPECT_PARAMS_DIFFER(a, b)                                            \
    do {                                                                      \
        const auto& _dm_pa = (a);                                             \
        const auto& _dm_pb = (b);                                             \
        if (_dm_pa.size() != _dm_pb.size()) {                                 \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_PARAMS_DIFFER(" #a ", " #b                     \
                    << ") failed: size mismatch " << _dm_pa.size()            \
                    << " vs " << _dm_pb.size();                               \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
        bool _dm_any = false;                                                 \
        for (std::size_t _dm_i = 0; _dm_i < _dm_pa.size(); ++_dm_i) {         \
            const double _dm_av =                                             \
                static_cast<double>(_dm_pa[_dm_i]);                           \
            const double _dm_bv =                                             \
                static_cast<double>(_dm_pb[_dm_i]);                           \
            const double _dm_d = _dm_av - _dm_bv;                             \
            const double _dm_abs = _dm_d < 0 ? -_dm_d : _dm_d;                \
            const double _dm_scale =                                          \
                (_dm_av < 0 ? -_dm_av : _dm_av) +                             \
                (_dm_bv < 0 ? -_dm_bv : _dm_bv);                              \
            const double _dm_thr = 1e-4 * (_dm_scale > 1.0 ? _dm_scale : 1.0);\
            if (_dm_abs > _dm_thr) { _dm_any = true; break; }                 \
        }                                                                     \
        if (!_dm_any) {                                                       \
            std::ostringstream _dm_oss;                                       \
            _dm_oss << "EXPECT_PARAMS_DIFFER(" #a ", " #b                     \
                    << ") failed: arrays match within tolerance";             \
            ::drum_machine_test::ThrowFailure(__FILE__, __LINE__,             \
                                              _dm_oss.str());                 \
        }                                                                     \
    } while (0)
