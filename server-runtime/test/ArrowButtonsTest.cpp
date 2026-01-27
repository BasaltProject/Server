#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <string>

namespace {

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;

[[nodiscard]] std::int32_t stateOf(const std::string& block, const std::map<std::string, std::string>& properties) {
    return registry::blockStateDefinitions::stateIdOf(basalt::wire::common::Identifier::vanilla(block), properties,
                                                      std::nullopt)
        .value_or(0);
}

} // namespace

TEST_CASE("only the families whose set says so answer to an arrow") {
    CHECK(arrowButtons::acceptsArrows("oak_button"));
    CHECK(arrowButtons::acceptsArrows("bamboo_button"));
    CHECK(arrowButtons::acceptsArrows("crimson_button"));
    // parity: `BlockSetType.STONE` and `POLISHED_BLACKSTONE` are the only two that say no and have a
    // button; iron, copper and gold say no too but never got one.
    CHECK_FALSE(arrowButtons::acceptsArrows("stone_button"));
    CHECK_FALSE(arrowButtons::acceptsArrows("polished_blackstone_button"));
    CHECK_FALSE(arrowButtons::acceptsArrows("stone"));
}

TEST_CASE("a button on the floor is two sixteenths tall, and on a wall it is against the wall") {
    const std::int32_t floorButton =
        stateOf("oak_button", {{"face", "floor"}, {"facing", "north"}, {"powered", "false"}});
    const std::array<double, 6> floorBox = arrowButtons::outline(floorButton);
    CHECK(floorBox[1] == doctest::Approx(0.0));
    CHECK(floorBox[4] == doctest::Approx(2.0));
    CHECK(floorBox[0] == doctest::Approx(5.0));
    CHECK(floorBox[3] == doctest::Approx(11.0));

    // parity: pressed, only one sixteenth is left sticking out.
    const std::int32_t pressed = stateOf("oak_button", {{"face", "floor"}, {"facing", "north"}, {"powered", "true"}});
    CHECK(arrowButtons::outline(pressed)[4] == doctest::Approx(1.0));

    // parity: a wall button looking east is stuck to the wall on its west, so its box is at x zero.
    const std::int32_t wallButton = stateOf("oak_button", {{"face", "wall"}, {"facing", "east"}, {"powered", "false"}});
    const std::array<double, 6> wallBox = arrowButtons::outline(wallButton);
    CHECK(wallBox[0] == doctest::Approx(0.0));
    CHECK(wallBox[3] == doctest::Approx(2.0));
}

TEST_CASE("an arrow reaches the button only where the button is") {
    const std::int32_t floorButton =
        stateOf("oak_button", {{"face", "floor"}, {"facing", "north"}, {"powered", "false"}});
    // Measured in `botonflecha`: on the floor it presses, half a block up it does not, even though
    // both are inside the same square.
    CHECK(arrowButtons::reaches(1, -60, 0, floorButton, 1.5, -60.0, 0.5, 0.25, 0.5));
    CHECK_FALSE(arrowButtons::reaches(1, -60, 8, floorButton, 1.5, -59.5, 8.5, 0.25, 0.5));

    const std::int32_t wallButton = stateOf("oak_button", {{"face", "wall"}, {"facing", "east"}, {"powered", "false"}});
    // And on a wall it is the sideways distance that decides, not the height.
    CHECK(arrowButtons::reaches(6, -60, 0, wallButton, 6.1, -59.6, 0.5, 0.25, 0.5));
    CHECK_FALSE(arrowButtons::reaches(6, -60, 4, wallButton, 6.5, -59.6, 4.5, 0.25, 0.5));
}
