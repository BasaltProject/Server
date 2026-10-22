#include "WorldItemUse.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>

#include <array>
#include <vector>

namespace basalt::server::runtime::plugins::itemUse {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;

constexpr std::array<play::Direction, 6> kFaces{play::Direction::Down,  play::Direction::Up,   play::Direction::North,
                                                play::Direction::South, play::Direction::West, play::Direction::East};

[[nodiscard]] world::neighbourUpdates::Context shapeContext(world::LoadedWorld& world, random::RandomSource& random) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::neighbourUpdates::Context{.world = world.state(),
                                            .worldName = world.name(),
                                            .minY = facts.chunkHeight.minY(),
                                            .maxY = facts.chunkHeight.maxY(),
                                            .random = random,
                                            .hasSkylight = facts.hasSkylight,
                                            .gameTime = world.gameTime()};
}

} // namespace

std::int32_t placeBlockItem(const blockUpdates::Target& target, const std::string_view item, const Click& click) {
    world::LoadedWorld& world = target.world;
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(std::string{item});
    if (!parsed.has_value() || click.face < 0 || click.face > 5) {
        return 0;
    }
    const play::Direction face = kFaces[static_cast<std::size_t>(click.face)];
    // parity: `BlockItem.getBlock`, and a wall variant when the click is on the side of a block.
    Identifier block = reg::itemBlocks::of(*parsed);
    if (face != play::Direction::Up && face != play::Direction::Down) {
        block = world::wallVariants::of(block).value_or(block);
    }
    const std::optional<std::int32_t> defaultState = reg::blocks::defaultStateIdOf(block);
    if (!defaultState.has_value() || !world.state().isLoaded(click.x >> 4, click.z >> 4)) {
        return 0;
    }
    // parity: `BlockPlaceContext.replacingClickedOnBlock`.
    const std::optional<std::int32_t> clicked = world.state().blockState(click.x, click.y, click.z);
    const Identifier* clickedBlock = clicked.has_value() ? reg::blockStateDefinitions::blockOf(*clicked) : nullptr;
    const bool replaceClicked = clicked.has_value() && reg::blockShapes::isReplaceable(*clicked) &&
                                clickedBlock != nullptr && *clickedBlock != block;
    const play::BlockPos beside = play::BlockPos{click.x, click.y, click.z}.offset(face);
    std::array<std::int32_t, 3> at{beside.x, beside.y, beside.z};
    if (replaceClicked) {
        at = {click.x, click.y, click.z};
    }
    const std::optional<std::int32_t> existing = world.state().blockState(at[0], at[1], at[2]);
    if (!existing.has_value() || !reg::blockShapes::isReplaceable(*existing)) {
        return 0;
    }

    random::RandomSource random = random::RandomSource::legacy(world.gameTime());
    const std::int32_t placed =
        world::blockPlacement::stateFor(block, *defaultState,
                                        world::blockPlacement::Placement{.face = face,
                                                                         .clickY = click.clickY,
                                                                         .clickX = click.clickX,
                                                                         .clickZ = click.clickZ,
                                                                         .yaw = click.yaw,
                                                                         .pitch = click.pitch,
                                                                         .replacing = *existing,
                                                                         .world = &world.state(),
                                                                         .worldName = world.name(),
                                                                         .x = at[0],
                                                                         .y = at[1],
                                                                         .z = at[2]},
                                        random);
    const std::int32_t first = world::doubleBlocks::firstHalf(placed);
    // parity: `BlockItem.canPlace`, a block never goes down inside somebody.
    std::vector<std::array<double, 6>> bodies;
    for (const player::PlayerSession* player : target.watchers) {
        if (player->gamemode != play::Gamemode::Spectator) {
            bodies.push_back(world::blockPlacement::playerBox(player->x, player->y, player->z, player->sneaking));
        }
    }
    if (world::blockPlacement::isObstructed(first, at[0], at[1], at[2], bodies) ||
        !world::blockSurvival::isSupported(world.state(), at[0], at[1], at[2], first)) {
        return 0;
    }
    const std::optional<world::SecondHalf> second = world::doubleBlocks::secondHalf(first, at[0], at[1], at[2]);
    if (second.has_value()) {
        const std::optional<std::int32_t> room = world.state().blockState(second->x, second->y, second->z);
        if (!room.has_value() || !reg::blockShapes::isReplaceable(*room)) {
            return 0;
        }
    }
    if (!world.state().setBlockState(at[0], at[1], at[2], first)) {
        return 0;
    }
    // parity: `getStateForPlacement` settles the new block against what is around it.
    std::int32_t settled = first;
    {
        random::RandomSource joining = random::RandomSource::legacy(world.gameTime());
        const world::neighbourUpdates::Context shapes = shapeContext(world, joining);
        settled = world::neighbourUpdates::connect(shapes, at[0], at[1], at[2], first).value_or(settled);
        static_cast<void>(
            world::rails::place(world.state(), world.name(), at[0], at[1], at[2],
                                [&settled, &at](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                                    if (x == at[0] && y == at[1] && z == at[2]) {
                                        settled = stateId;
                                    }
                                }));
        settled = world::redstoneWire::shapeOnPlacement(world.state(), at[0], at[1], at[2], settled).value_or(settled);
    }
    std::int32_t put = 1;
    if (second.has_value() && world.state().setBlockState(second->x, second->y, second->z, second->stateId)) {
        ++put;
    }
    if (reg::blockEntityTypes::ofBlock(block) != nullptr) {
        if (server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4); chunk != nullptr) {
            chunk->setBlockEntity(server::world::ChunkBlockEntity{at[0] & 15, at[1], at[2] & 15, block,
                                                                  world::blockEntityDefaults::of(block)});
        }
    }
    // parity: `DiodeBlock.setPlacedBy` and `TripWireHookBlock.setPlacedBy`.
    world::redstoneComponents::bookOnPlacement(world.state(), world.name(), at[0], at[1], at[2],
                                               world::comparatorReading::of(world.state()));
    if (block.path() == "tripwire_hook") {
        std::vector<world::BlockChange> hookChanges;
        world::tripwires::calculateHook(world.state(), at[0], at[1], at[2], world.gameTime(), hookChanges);
    }
    for (const player::PlayerSession* watcher : target.watchers) {
        player::sendBlockChangeTo(*watcher, at[0], at[1], at[2], settled);
        if (second.has_value()) {
            player::sendBlockChangeTo(*watcher, second->x, second->y, second->z, second->stateId);
        }
    }
    if (target.blocks != nullptr) {
        target.blocks->settleBlock(world, target.watchers, at[0], at[1], at[2], std::nullopt,
                                   world::neighbourUpdates::phase::kAll);
    }
    return put;
}

} // namespace basalt::server::runtime::plugins::itemUse
