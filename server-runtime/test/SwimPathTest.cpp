#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace swimPath = basalt::server::runtime::entity::swimPath;
namespace ai = basalt::server::runtime::entity::ai;
namespace registry = basalt::server::registry;

namespace {

struct Pool {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};

    /// Water from x 0 to 10, y 60 to 63 and z 0 to 3, air above it, stone around.
    Pool() {
        for (std::int32_t chunkX = -1; chunkX <= 0; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 0; ++chunkZ) {
                state.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
        for (std::int32_t x = -1; x <= 11; ++x) {
            for (std::int32_t z = -1; z <= 4; ++z) {
                for (std::int32_t y = 59; y <= 66; ++y) {
                    const bool inside = x >= 0 && x <= 10 && z >= 0 && z <= 3 && y >= 60;
                    put(x, y, z, !inside ? "stone" : (y <= 63 ? "water" : "air"));
                }
            }
        }
    }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, const std::string& path) {
        const registry::BlockState* block = registry::blocks::named(path);
        REQUIRE(block != nullptr);
        static_cast<void>(state.setBlockState(x, y, z, block->stateId));
        REQUIRE(state.blockState(x, y, z) == block->stateId);
    }
};

} // namespace

TEST_CASE("a swim path goes round a wall through the water") {
    Pool pool;
    for (std::int32_t y = 60; y <= 63; ++y) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            pool.put(5, y, z, "stone");
        }
    }
    const auto path = swimPath::find(pool.state, {1, 61, 0}, {9, 61, 0}, false);
    REQUIRE(path.has_value());
    REQUIRE_FALSE(path->empty());
    bool passedTheGap = false;
    for (const auto& node : *path) {
        CHECK(swimPath::isSwimmable(pool.state, node[0], node[1], node[2]));
        passedTheGap = passedTheGap || (node[0] == 5 && node[2] == 3);
    }
    CHECK(passedTheGap);
    CHECK(std::abs(path->back()[0] - 9) + std::abs(path->back()[1] - 61) + std::abs(path->back()[2]) <= 1);
}

TEST_CASE("only a breaching swimmer may use the air on the surface") {
    Pool pool;
    CHECK(swimPath::isBreach(pool.state, 3, 64, 1));
    CHECK_FALSE(swimPath::isBreach(pool.state, 3, 65, 1));
    for (std::int32_t z = 0; z <= 3; ++z) {
        for (std::int32_t y = 60; y <= 63; ++y) {
            pool.put(5, y, z, "stone");
        }
    }
    const auto dry = swimPath::find(pool.state, {1, 61, 1}, {9, 61, 1}, false);
    REQUIRE(dry.has_value());
    CHECK(dry->back()[0] < 5);
    // parity: a breach needs water under it, so the top of a wall is no way across either.
    const auto over = swimPath::find(pool.state, {1, 61, 1}, {9, 61, 1}, true);
    REQUIRE(over.has_value());
    CHECK(over->back()[0] < 5);
}

TEST_CASE("a dolphin swims along its path towards the target") {
    Pool pool;
    ServerEntity dolphin{1, Uuid{1, 1}, Identifier::vanilla("dolphin"), 0, 1.5, 61.0, 1.5};
    REQUIRE(ai::swimsSmoothly(dolphin));
    REQUIRE(ai::navigation::moveTo(pool.state, dolphin, 9.5, 61.0, 1.5, 1.0));
    for (std::int32_t tick = 0; tick < 3000 && !ai::navigation::isDone(dolphin); ++tick) {
        ai::navigation::tick(pool.state, dolphin);
        ai::moveControl::tick(pool.state, dolphin, 1.2);
    }
    CHECK(dolphin.x > 7.5);
    CHECK(dolphin.y > 59.5);
}
