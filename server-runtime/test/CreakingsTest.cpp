#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/entity/Creakings.hpp>

#include <doctest.h>

namespace creakings = basalt::server::runtime::entity::creakings;
using basalt::wire::common::Uuid;
using basalt::wire::nbt::NbtCompound;

TEST_CASE("a heart saves its creaking as vanilla's four-int uuid") {
    NbtCompound heart;
    const Uuid creaking{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    creakings::setHeartCreaking(heart, creaking);
    CHECK(heart.getIntArray("creaking").size() == 4);
    CHECK(creakings::heartCreaking(heart) == creaking);
    creakings::setHeartCreaking(heart, std::nullopt);
    CHECK_FALSE(creakings::heartCreaking(heart).has_value());
}

TEST_CASE("a heart's comparator reads fifteen at the heart and nothing past thirty-two blocks") {
    // parity: `15 - floor(clamp(d, 0, 32) / 32 * 15)`.
    CHECK(creakings::signalFor(0.0) == 15);
    CHECK(creakings::signalFor(16.0) == 8);
    CHECK(creakings::signalFor(32.0) == 0);
    CHECK(creakings::signalFor(40.0) == 0);
}
