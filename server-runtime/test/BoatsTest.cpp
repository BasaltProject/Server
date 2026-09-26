#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <memory>
#include <string>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace boats = basalt::server::runtime::entity::boats;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::unique_ptr<ServerEntity> spawn(std::int32_t id, const std::string& type, double x, double y,
                                                  double z) {
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla(type));
    REQUIRE(entry != nullptr);
    return std::make_unique<ServerEntity>(id, Uuid{static_cast<std::uint64_t>(id), 1}, entry->id, entry->protocolId, x,
                                          y, z);
}

/// A pond: stone at 62, still water at 63.
struct Pond {
    WorldState world = WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};

    Pond() {
        world.getOrGenerateChunk(0, 0);
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                REQUIRE(world.setBlockState(x, 62, z, registry::blocks::named("stone")->stateId));
                REQUIRE(world.setBlockState(x, 63, z, registry::blocks::named("water")->stateId));
            }
        }
    }
};

} // namespace

TEST_CASE("a boat dropped over water settles on it") {
    Pond pond;
    const std::unique_ptr<ServerEntity> boat = spawn(1, "oak_boat", 8.0, 65.0, 8.0);
    for (std::int32_t turn = 0; turn < 60; ++turn) {
        static_cast<void>(boats::tick(pond.world, *boat, false));
    }
    REQUIRE(boat->boat != nullptr);
    CHECK(boat->boat->status == boats::Status::InWater);
    // parity: it rests with its bottom in the water, below the surface at eight ninths.
    CHECK(boat->y > 63.0);
    CHECK(boat->y < 63.0 + (8.0 / 9.0));
    CHECK(boats::maxPassengers(Identifier::vanilla("oak_chest_boat")) == 1);
}
