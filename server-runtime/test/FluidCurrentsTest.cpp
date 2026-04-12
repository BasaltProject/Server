#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/FluidCurrents.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>

#include <doctest.h>

#include <map>
#include <tuple>

namespace currents = basalt::server::runtime::world::fluidCurrents;
namespace fluidFlow = basalt::server::runtime::world::fluidFlow;
using basalt::server::runtime::entity::Aabb;
using basalt::server::runtime::world::FluidKind;

namespace {

/// A strip of water running east from a source: levels 0, 1, 2 and 3 on an air floor that nothing
/// fills, which is what the flow reads as the ground.
struct Strip {
    std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::int32_t> blocks;

    Strip() {
        for (std::int32_t level = 0; level < 4; ++level) {
            blocks[{level, 64, 0}] = basalt::server::runtime::world::baseStateId(FluidKind::Water) + level;
        }
    }

    [[nodiscard]] currents::BlockAt at() const {
        return [this](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
            const auto found = blocks.find({x, y, z});
            return found == blocks.end() ? 0 : found->second;
        };
    }
};

} // namespace

TEST_CASE("a flow runs from the higher square to the lower one") {
    const Strip strip;
    const std::array<double, 3> flow = currents::flowAt(
        strip.at(), 1, 64, 0, basalt::server::runtime::world::baseStateId(FluidKind::Water) + 1, nullptr);
    CHECK(flow[0] == doctest::Approx(1.0));
    CHECK(flow[1] == doctest::Approx(0.0));
    CHECK(flow[2] == doctest::Approx(0.0));
}

TEST_CASE("an item in the flow is carried downstream, a player by the average") {
    const Strip strip;
    const currents::Contact contact = currents::touch(strip.at(), Aabb::of(1.5, 64.0, 0.5, 0.25, 0.25), true, nullptr);
    CHECK(contact.inWater());
    CHECK_FALSE(contact.inLava());

    std::array<double, 3> item{};
    currents::pushAll(item, contact, false, false);
    CHECK(item[0] == doctest::Approx(currents::kWaterScale));
    CHECK(item[2] == doctest::Approx(0.0));

    // parity: something already moving keeps the plain push, however weak.
    std::array<double, 3> moving{0.5, 0.0, 0.0};
    currents::pushAll(moving, contact, true, false);
    CHECK(moving[0] > 0.5);
}

TEST_CASE("a box not pushed by fluid is still in it, and carried by nothing") {
    const Strip strip;
    const currents::Contact contact = currents::touch(strip.at(), Aabb::of(1.5, 64.0, 0.5, 0.25, 0.25), false, nullptr);
    CHECK(contact.inWater());
    std::array<double, 3> movement{};
    currents::pushAll(movement, contact, false, false);
    CHECK(movement[0] == 0.0);
}
