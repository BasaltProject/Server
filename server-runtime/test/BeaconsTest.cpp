#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/Conduits.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <tuple>

using namespace basalt::server::runtime::world;

namespace {

/// A pyramid `levels` high under the beacon at the origin, and nothing else.
[[nodiscard]] beacons::IsBaseBlock pyramidOf(std::int32_t levels) {
    return [levels](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
        const std::int32_t step = -y;
        return step >= 1 && step <= levels && std::abs(x) <= step && std::abs(z) <= step;
    };
}

} // namespace

TEST_CASE("a pyramid is counted outwards and stops at the first ring with a hole") {
    for (std::int32_t built = 0; built <= 4; ++built) {
        CHECK(beacons::levelsUnder(pyramidOf(built), 0, 0, 0, -64) == built);
    }
    // parity: `MAX_LEVELS`, which no taller pyramid gets past.
    CHECK(beacons::levelsUnder(pyramidOf(6), 0, 0, 0, -64) == 4);
}

TEST_CASE("a pyramid that runs out of world is only as tall as it got") {
    // The floor is two below, so only one ring fits whatever is built.
    CHECK(beacons::levelsUnder(pyramidOf(4), 0, 0, 0, -1) == 1);
}

TEST_CASE("what a beacon may offer grows with the pyramid") {
    CHECK_FALSE(beacons::allows("minecraft:speed", "", 0));
    CHECK(beacons::allows("minecraft:speed", "", 1));
    CHECK_FALSE(beacons::allows("minecraft:resistance", "", 1));
    CHECK(beacons::allows("minecraft:resistance", "", 2));
    CHECK_FALSE(beacons::allows("minecraft:strength", "", 2));
    CHECK(beacons::allows("minecraft:strength", "", 3));
}

TEST_CASE("regeneration is only ever the second effect") {
    // parity: the fourth tier is offered as a second effect and never as the first.
    CHECK_FALSE(beacons::allows("minecraft:regeneration", "", 4));
    CHECK(beacons::allows("minecraft:speed", "minecraft:regeneration", 4));
}

TEST_CASE("a second effect needs the whole pyramid, and is the first one again or regeneration") {
    CHECK_FALSE(beacons::allows("minecraft:speed", "minecraft:haste", 3));
    CHECK_FALSE(beacons::allows("minecraft:speed", "minecraft:haste", 4));
    CHECK(beacons::allows("minecraft:speed", "minecraft:speed", 4));
    CHECK(beacons::allows("minecraft:speed", "minecraft:regeneration", 4));
}

TEST_CASE("an effect nobody offers is refused at every height") {
    for (std::int32_t levels = 0; levels <= 4; ++levels) {
        CHECK_FALSE(beacons::allows("minecraft:wither", "", levels));
    }
}

TEST_CASE("the beam reaches ten blocks a level and lasts nine seconds plus two") {
    const beacons::Beam one = beacons::beamOf(1, false);
    CHECK(one.range == doctest::Approx(20.0));
    CHECK(one.durationTicks == 11 * 20);
    CHECK(one.amplifier == 0);

    const beacons::Beam full = beacons::beamOf(4, false);
    CHECK(full.range == doctest::Approx(50.0));
    CHECK(full.durationTicks == 17 * 20);
    CHECK(full.amplifier == 0);

    // parity: the same effect twice is what buys the second level of it.
    CHECK(beacons::beamOf(4, true).amplifier == 1);
    CHECK(beacons::beamOf(3, true).amplifier == 0);
}

TEST_CASE("every effect a beacon offers is offered exactly once") {
    std::set<std::string_view> seen;
    for (const std::span<const std::string_view>& tier : beacons::effectTiers()) {
        for (const std::string_view effect : tier) {
            CHECK(seen.insert(effect).second);
        }
    }
    CHECK(seen.size() == 6);
}

TEST_CASE("the beam stops at the first square that dampens all the light") {
    CHECK(beacons::letsBeamThrough(0, "minecraft:air"));
    CHECK(beacons::letsBeamThrough(0, "minecraft:short_grass"));
    CHECK(beacons::letsBeamThrough(1, "minecraft:water"));
    CHECK_FALSE(beacons::letsBeamThrough(15, "minecraft:stone"));
    // parity: bedrock stops all the light and the beam goes through it anyway.
    CHECK(beacons::letsBeamThrough(15, "minecraft:bedrock"));
}

TEST_CASE("the beam gets out only when every square above the beacon lets it") {
    const auto clear = [](std::int32_t, std::int32_t, std::int32_t) { return true; };
    CHECK(beacons::beamReachesSky(clear, 0, 64, 0, 80));

    // The beacon's own square is never asked: the walk starts one above it.
    const auto roofAt = [](std::int32_t, const std::int32_t y, std::int32_t) { return y != 70; };
    CHECK_FALSE(beacons::beamReachesSky(roofAt, 0, 64, 0, 80));
    CHECK(beacons::beamReachesSky(roofAt, 0, 71, 0, 80));
}

TEST_CASE("the beam reaches its own square inflated sideways and open at the top") {
    const beacons::Beam one = beacons::beamOf(1, false);
    CHECK(beacons::reaches(one, 0, 64, 0, 20.0, 64.0, 0.5));
    CHECK_FALSE(beacons::reaches(one, 0, 64, 0, 21.5, 64.0, 0.5));
    // parity: the block's own square is inflated, so the far side reaches one further.
    CHECK(beacons::reaches(one, 0, 64, 0, 21.0, 64.0, 0.5));
    // parity: as high as the world goes, and only the reach downwards.
    CHECK(beacons::reaches(one, 0, 64, 0, 0.5, 300.0, 0.5));
    CHECK(beacons::reaches(one, 0, 64, 0, 0.5, 44.0, 0.5));
    CHECK_FALSE(beacons::reaches(one, 0, 64, 0, 0.5, 43.0, 0.5));
}

TEST_CASE("a conduit's frame is the three rings around it, and only under water") {
    namespace conduits = basalt::server::runtime::world::conduits;
    const auto water = [](std::int32_t, std::int32_t, std::int32_t) { return true; };
    const auto everything = [](std::int32_t, std::int32_t, std::int32_t) { return true; };

    // parity: `ConduitBlockEntity.updateShape`: three rings of sixteen share their four corners
    // pairwise, forty-two in all, which is also the size it starts hunting at.
    CHECK(conduits::frame(water, everything, 0, 64, 0).size() == conduits::kMinKillSize);

    const auto dryCorner = [](std::int32_t x, std::int32_t y, std::int32_t z) {
        return !(x == 1 && y == 65 && z == 1);
    };
    CHECK(conduits::frame(dryCorner, everything, 0, 64, 0).empty());

    CHECK(conduits::effectRange(16) == 32);
    CHECK(conduits::effectRange(42) == 96);
    CHECK(conduits::closerThan({0, 0, 0}, {8, 0, 0}, 8.5));
    CHECK_FALSE(conduits::closerThan({0, 0, 0}, {8, 0, 0}, 8.0));
}
