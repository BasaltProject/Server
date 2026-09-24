#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::runtime::entity;
using basalt::server::registry::BlockFace;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

struct Loaded {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

[[nodiscard]] ServerEntity shulkerAt(double x, double y, double z) {
    return ServerEntity{1, basalt::wire::common::Uuid::random(), Identifier::vanilla("shulker"), 1, x, y, z};
}

const std::vector<ServerEntity*> kNobody;

} // namespace

TEST_CASE("a shulker holds on the first sturdy face with room for its lid") {
    Loaded loaded;
    ServerEntity shulker = shulkerAt(4.5, 64.0, 4.5);
    CHECK_FALSE(shulkers::attachableSurface(loaded.state, shulker, kNobody, {4, 64, 4}).has_value());
    REQUIRE(loaded.state.setBlockState(5, 64, 4, stateOf("stone")));
    CHECK(shulkers::attachableSurface(loaded.state, shulker, kNobody, {4, 64, 4}) == BlockFace::East);
    REQUIRE(loaded.state.setBlockState(4, 63, 4, stateOf("stone")));
    // parity: `Direction.values()` order puts down first.
    CHECK(shulkers::attachableSurface(loaded.state, shulker, kNobody, {4, 64, 4}) == BlockFace::Down);
    // A block where the open lid would go leaves no room on that face.
    REQUIRE(loaded.state.setBlockState(4, 65, 4, stateOf("stone")));
    CHECK_FALSE(shulkers::canStayAt(loaded.state, shulker, kNobody, {4, 64, 4}, BlockFace::Down));
}

TEST_CASE("the lid eases a twentieth a tick towards its peek") {
    shulkers::ShulkerState state;
    state.peek = 30;
    std::int32_t ticks = 0;
    while (shulkers::updatePeek(state)) {
        ++ticks;
    }
    CHECK(ticks == 6);
    CHECK(state.currentPeek == doctest::Approx(0.3F));
    CHECK(shulkers::physicalPeek(0.0F) == doctest::Approx(0.0F));
    CHECK(shulkers::physicalPeek(1.0F) == doctest::Approx(1.0F));
}

TEST_CASE("a bullet heads along one axis towards its target at 0.15 a tick") {
    Loaded loaded;
    ServerEntity bullet{2,  basalt::wire::common::Uuid::random(), Identifier::vanilla("shulker_bullet"), 1, 4.5, 64.5,
                        4.5};
    shulkers::BulletState state;
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(7);
    shulkers::selectNextMoveDirection(loaded.state, bullet, state, 1, std::array<double, 4>{12.5, 64.0, 4.5, 1.8},
                                      random);
    REQUIRE(state.direction.has_value());
    // Only east brings it nearer without moving vertically, which the shulker's axis forbids.
    CHECK(*state.direction == BlockFace::East);
    CHECK(state.targetDeltaX == doctest::Approx(0.15));
    CHECK(state.flightSteps >= 10);
    CHECK(state.flightSteps <= 50);
}
