#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/config/AnimationsConfig.hpp>
#include <basalt/server/runtime/placeholder/AnimationPlaceholders.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>

#include <doctest.h>

#include <cstdint>

using basalt::server::runtime::config::Animation;
using basalt::server::runtime::config::AnimationsConfig;
using basalt::wire::common::placeholder::PlaceholderContext;
using basalt::wire::common::placeholder::PlaceholderKind;
using basalt::wire::common::placeholder::PlaceholderRegistry;
using basalt::wire::common::placeholder::PlaceholderValue;
namespace placeholder = basalt::server::runtime::placeholder;

namespace {

struct Fixture {
    AnimationsConfig animations{
        {Animation{.name = "spinner", .frames = {"|", "/", "-"}, .changeIntervalMillis = 100},
         Animation{.name = "live", .frames = {"players: %basalt_test_online%"}, .changeIntervalMillis = 100},
         Animation{.name = "nested", .frames = {"[%basalt_animation_spinner%]"}, .changeIntervalMillis = 100}}};
    PlaceholderRegistry registry;
    std::int64_t now = 0;

    Fixture() {
        registry.registerProvider("basalt_test", [](std::string_view, const PlaceholderContext&) {
            return PlaceholderValue::of(std::string{"4"});
        });
    }

    [[nodiscard]] basalt::wire::common::placeholder::PlaceholderProvider provider() {
        return placeholder::animationPlaceholders(placeholder::AnimationPlaceholderSources{
            .animations = [this]() -> const AnimationsConfig& { return animations; },
            .registry = [this]() -> const PlaceholderRegistry& { return registry; },
            .nowMillis = [this] { return now; },
        });
    }
};

} // namespace

TEST_CASE("an animation shows the frame the wall clock is on") {
    Fixture fixture;
    const auto provider = fixture.provider();
    CHECK(provider("spinner", PlaceholderContext{}).text == "|");
    fixture.now = 100;
    CHECK(provider("spinner", PlaceholderContext{}).text == "/");
    fixture.now = 250;
    CHECK(provider("spinner", PlaceholderContext{}).text == "-");
    // Everyone looking at it at the same moment sees the same frame, so it wraps rather than ends.
    fixture.now = 300;
    CHECK(provider("spinner", PlaceholderContext{}).text == "|");
}

TEST_CASE("a frame may carry live data of its own") {
    Fixture fixture;
    CHECK(fixture.provider()("live", PlaceholderContext{}).text == "players: 4");
}

TEST_CASE("an animation inside an animation is blanked rather than resolved") {
    Fixture fixture;
    // parity of intent with the Kotlin build: two frames naming each other would recurse until the
    // stack ran out, so the inner name never reaches the registry.
    CHECK(fixture.provider()("nested", PlaceholderContext{}).text == "[]");
}

TEST_CASE("an animation nobody configured names the file to add it to") {
    Fixture fixture;
    const PlaceholderValue missing = fixture.provider()("absent", PlaceholderContext{});
    CHECK(missing.kind == PlaceholderKind::Unavailable);
    REQUIRE(missing.reason.has_value());
    CHECK(missing.reason->find("animations.toml") != std::string::npos);

    CHECK(fixture.provider()("", PlaceholderContext{}).kind == PlaceholderKind::Unknown);
}
