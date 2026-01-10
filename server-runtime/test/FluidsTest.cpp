#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

} // namespace

TEST_CASE("only water and lava count as being in something") {
    CHECK(fluids::of(stateOf("water")) == Submersion::Water);
    CHECK(fluids::of(stateOf("lava")) == Submersion::Lava);
    CHECK(fluids::of(registry::blocks::air().stateId) == Submersion::None);
    CHECK(fluids::of(stateOf("stone")) == Submersion::None);
    // A flowing fluid is the same fluid.
    const std::optional<std::int32_t> flowing =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("water"), {{"level", "3"}}, stateOf("water"));
    REQUIRE(flowing.has_value());
    CHECK(fluids::of(*flowing) == Submersion::Water);
    // And a block that merely holds water is not being in water.
    const std::optional<std::int32_t> logged = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_slab"), {{"type", "bottom"}, {"waterlogged", "true"}}, stateOf("oak_slab"));
    REQUIRE(logged.has_value());
    CHECK(fluids::of(*logged) == Submersion::None);
}

TEST_CASE("the eyes are lower when crouching") {
    CHECK(fluids::eyeHeight(false) == doctest::Approx(1.62));
    CHECK(fluids::eyeHeight(true) == doctest::Approx(1.27));
    CHECK(fluids::eyeHeight(true) < fluids::eyeHeight(false));
}

TEST_CASE("air runs out one tick at a time and comes back four") {
    // parity: `decreaseAirSupply` and `increaseAirSupply`.
    CHECK(fluids::airAfterTick(300, true) == 299);
    CHECK(fluids::airAfterTick(0, true) == -1);
    CHECK(fluids::airAfterTick(-19, true) == -20);
    CHECK(fluids::airAfterTick(100, false) == 104);
    // And it never goes past full, however little is missing.
    CHECK(fluids::airAfterTick(299, false) == 300);
    CHECK(fluids::airAfterTick(300, false) == 300);
    CHECK(fluids::airAfterTick(297, false) == 300);
}

TEST_CASE("the numbers a drowning or burning player is measured against") {
    CHECK(fluids::kMaxAir == 300);
    // parity: twenty ticks past empty before the first hit.
    CHECK(fluids::kDrowningAir == -20);
    CHECK(fluids::kDrownDamage == doctest::Approx(2.0F));
    CHECK(fluids::kLavaDamage == doctest::Approx(4.0F));
    CHECK(fluids::kFireDamage == doctest::Approx(1.0F));
    // parity: fifteen seconds alight, counted in ticks.
    CHECK(fluids::kLavaFireTicks == 300);
}
