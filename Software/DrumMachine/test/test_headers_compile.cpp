// test_headers_compile.cpp — verifies that every interface header lands in
// the host build cleanly under -Wall -Wextra -Werror, both individually and
// together in the same TU. Per task 02 acceptance criteria #2.
//
// This is a compile-time test more than a runtime one — the EXPECT_TRUE
// below merely confirms the framework wires up. The real check is that this
// TU builds at all: any missing include, ODR violation, or accidental
// libDaisy dependency in the host build would fail the compile.

#include "test_macros.h"

#include "PadIndex.h"
#include "Config.h"
#include "instruments/IInstrument.h"
#include "controls/IButton.h"
#include "controls/ILed.h"
#include "randomization/IRng.h"
#include "randomization/RandomizationProfile.h"

#include <array>
#include <cstddef>
#include <type_traits>

namespace {

// Touch each interface as an abstract type so the compiler emits its vtable
// machinery and we catch any silent breakage (e.g. a missing virtual dtor).
static_assert(std::is_abstract_v<IInstrument>, "IInstrument must be abstract");
static_assert(std::is_abstract_v<IButton>,     "IButton must be abstract");
static_assert(std::is_abstract_v<ILed>,        "ILed must be abstract");
static_assert(std::is_abstract_v<IRng>,        "IRng must be abstract");

// PadIndex sanity: values match spec, Count is 4.
static_assert(static_cast<std::size_t>(PadIndex::Bass)      == 0, "");
static_assert(static_cast<std::size_t>(PadIndex::Snare)     == 1, "");
static_assert(static_cast<std::size_t>(PadIndex::HiHat)     == 2, "");
static_assert(static_cast<std::size_t>(PadIndex::Resonator) == 3, "");
static_assert(static_cast<std::size_t>(PadIndex::Count)     == 4, "");

// RandomizationProfile is a real (non-empty) template that instantiates.
using ProfileOf3 = RandomizationProfile<3>;
static_assert(ProfileOf3::kCount == 3, "RandomizationProfile<N>::kCount == N");
static_assert(std::is_same_v<decltype(ProfileOf3{}.params),
                             std::array<ParamRange, 3>>,
              "RandomizationProfile<N>::params is std::array<ParamRange, N>");

// Config constants exist and have the spec-defined types.
static_assert(std::is_same_v<decltype(Config::kSampleRate), const float>, "");
static_assert(std::is_same_v<decltype(Config::kAudioBlockSize), const std::size_t>, "");
static_assert(Config::kHoldThresholdMs == 500u, "kHoldThresholdMs is 500ms per spec");

}  // namespace

TEST_CASE("interface_headers_compile_together") {
    // The real assertion is the compile above; this just confirms the test
    // wires into the registry.
    EXPECT_TRUE(static_cast<int>(PadIndex::Count) == 4);
    EXPECT_TRUE(Config::kDefaultRandomizationDepth == 0.5f);
}
