#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <utility>
#include <vector>

using basalt::server::runtime::entity::ServerEntity;
using basalt::server::runtime::world::WorldState;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;
namespace doors = basalt::server::runtime::entity::ai::doorInteraction;

TEST_CASE("a mob opens the door on its path and shuts it once it is through") {
    WorldState world;
    world.getOrGenerateChunk(0, 0);
    const std::optional<std::int32_t> closed = basalt::server::registry::blocks::stateIdOf(
        Identifier::vanilla("oak_door"), {{"half", "lower"}, {"open", "false"}, {"facing", "east"}});
    REQUIRE(closed.has_value());
    REQUIRE(world.setBlockState(5, 64, 5, *closed));

    const basalt::server::registry::EntityTypeEntry* entry =
        basalt::server::registry::entityTypes::of(Identifier::vanilla("copper_golem"));
    REQUIRE(entry != nullptr);
    ServerEntity mob{1, Uuid{1, 1}, entry->id, entry->protocolId, 4.5, 64.0, 5.5};
    mob.path = std::vector<std::array<std::int32_t, 3>>{{3, 64, 5}, {4, 64, 5}, {5, 64, 5}, {6, 64, 5}, {7, 64, 5}};
    mob.pathIndex = 2;

    std::vector<std::pair<doors::Block, bool>> changes;
    const doors::SetDoor record = [&](const doors::Block& pos, bool open) {
        changes.emplace_back(pos, open);
        const std::optional<std::int32_t> state = basalt::server::registry::blocks::stateIdOf(
            Identifier::vanilla("oak_door"),
            {{"half", "lower"}, {"open", open ? "true" : "false"}, {"facing", "east"}});
        REQUIRE(world.setBlockState(pos[0], pos[1], pos[2], *state));
    };
    doors::State state;
    doors::tick(world, mob, state, {}, record);
    REQUIRE(changes.size() == 1);
    CHECK(changes.front() == std::pair{doors::Block{5, 64, 5}, true});

    // Two nodes on and still close by: the door behind it is shut.
    mob.x = 6.5;
    mob.pathIndex = 4;
    doors::tick(world, mob, state, {}, record);
    REQUIRE(changes.size() == 2);
    CHECK(changes.back() == std::pair{doors::Block{5, 64, 5}, false});
    CHECK(state.doorsToClose.empty());
}
