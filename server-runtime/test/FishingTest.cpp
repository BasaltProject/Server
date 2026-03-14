#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/FishingHooks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

#include <cstdint>

namespace hooks = basalt::server::runtime::entity::fishingHooks;
using basalt::server::random::RandomSource;
using basalt::server::runtime::entity::ServerEntity;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

[[nodiscard]] ServerEntity bobber() {
    return ServerEntity{1, Uuid{1, 1}, Identifier::vanilla("fishing_bobber"), 0, 0.5, 62.9, 0.5};
}

} // namespace

TEST_CASE("a cast flies the way the player looks, about a block a tick") {
    ServerEntity hook = bobber();
    hooks::HookState state;
    state.random = RandomSource::legacy(3);
    hooks::cast(hook, state, 0.0, 65.62, 0.0, 0.0F, 0.0F);
    // Facing south, which is +z, from a third of a block in front of the eyes.
    CHECK(hook.z == doctest::Approx(0.3).epsilon(0.001));
    // parity: 0.6 over the length plus a roll around a half, so a level cast is about 1.1.
    CHECK(hook.velocityZ > 1.08);
    CHECK(hook.velocityZ < 1.12);
    CHECK(hook.velocityX == doctest::Approx(0.0).epsilon(0.001));
}

TEST_CASE("open water is water below and air above, five blocks wide") {
    using Layer = hooks::WaterLayer;
    const auto lake = [](std::int32_t, std::int32_t y, std::int32_t) {
        return y <= 62 ? Layer::InsideWater : Layer::AboveWater;
    };
    CHECK(hooks::calculateOpenWater(0, 62, 0, lake));
    // A dock beside the bobber spoils its layer.
    const auto dock = [&lake](std::int32_t x, std::int32_t y, std::int32_t z) {
        return x == 2 && y == 63 ? Layer::Invalid : lake(x, y, z);
    };
    CHECK_FALSE(hooks::calculateOpenWater(0, 62, 0, dock));
    // A bobber hanging over the water has air for its lowest layer, which is never open water.
    CHECK_FALSE(hooks::calculateOpenWater(0, 64, 0, lake));
}

TEST_CASE("a fish comes, bites for one to two seconds, and goes") {
    ServerEntity hook = bobber();
    hooks::HookState state;
    state.random = RandomSource::legacy(11);
    const hooks::Surroundings around{.isWater = [](std::int32_t, std::int32_t, std::int32_t) { return true; }};
    std::int32_t ticks = 0;
    while (!state.biting && ticks < 1000) {
        hooks::catchingFish(hook, state, around);
        ++ticks;
    }
    REQUIRE(state.biting);
    // parity: at most thirty seconds of luring and four of circling.
    CHECK(ticks <= 600 + 80 + 2);
    CHECK((state.nibble >= 20 && state.nibble <= 40));
    std::int32_t held = 0;
    while (state.biting) {
        hooks::catchingFish(hook, state, around);
        ++held;
    }
    CHECK((held >= 20 && held <= 40));
    CHECK(state.timeUntilLured == 0);
}
