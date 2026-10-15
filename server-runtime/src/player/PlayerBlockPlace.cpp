#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <array>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using play::BlockPos;
using play::Gamemode;
using wire::common::Identifier;

[[nodiscard]] std::int64_t seedOf(const BreakContext& context) { return context.nextSeed ? context.nextSeed() : 0; }

[[nodiscard]] storage::StoredItemStack withCount(const storage::StoredItemStack& stack, const std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

[[nodiscard]] world::neighbourUpdates::Context contextOf(world::LoadedWorld& world, random::RandomSource& random) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::neighbourUpdates::Context{
        .world = world.state(),
        .worldName = world.name(),
        .minY = facts.chunkHeight.minY(),
        .maxY = facts.chunkHeight.maxY(),
        .random = random,
        // parity: `water_evaporates`.
        .evaporates = world.declaration().dimension == config::WorldDimension::Nether,
        .hasSkylight = facts.hasSkylight,
        .ultrawarm = world.declaration().dimension == config::WorldDimension::Nether,
        // parity: `PotentSulfurBlock.updateShape` is `validBlockState`.
        .geyser =
            [&world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t stateId) {
                return world::geysers::validState(world.state(), world.gameTime(), x, y, z, stateId);
            },
        .gameTime = world.gameTime(),
        .tntExplodes = world.gameRules().get(world::gameRules::kTntExplodes)};
}

/// parity: `BlockItem.canPlace`.
[[nodiscard]] std::vector<std::array<double, 6>>
buildersInTheWay(const BreakContext& context, const PlayerSession& placer, world::LoadedWorld& world) {
    std::vector<std::array<double, 6>> boxes;
    bool sawSelf = false;
    for (const PlayerSession* player : playersIn(context, world.name())) {
        sawSelf = sawSelf || player->entityId() == placer.entityId();
        if (player->gamemode == Gamemode::Spectator) {
            continue;
        }
        boxes.push_back(world::blockPlacement::playerBox(player->x, player->y, player->z, player->sneaking));
    }
    if (!sawSelf && placer.gamemode != Gamemode::Spectator) {
        boxes.push_back(world::blockPlacement::playerBox(placer.x, placer.y, placer.z, placer.sneaking));
    }
    return boxes;
}

} // namespace

bool placeBlockFor(PlayerSession& player, world::LoadedWorld& world, const BlockHit& hit, const Identifier& heldBlock,
                   const std::int32_t heldSlot, const storage::StoredItemStack& held, const PlaceContext& context) {
    const play::BlockHitResult hitResult{.pos = BlockPos{hit.x, hit.y, hit.z},
                                         .direction = hit.direction,
                                         .clickX = hit.clickX,
                                         .clickY = hit.clickY,
                                         .clickZ = hit.clickZ,
                                         .inside = hit.inside};
    PlayerSession* session = &player;
    const std::optional<std::int32_t> clickedState = world.state().blockState(hit.x, hit.y, hit.z);
    // parity: `BlockItem` on a wall.
    const bool againstAWall = hit.direction != play::Direction::Up && hit.direction != play::Direction::Down;
    // parity: `BlockItem.getBlock`.
    Identifier placedBlock = reg::itemBlocks::of(heldBlock);
    if (againstAWall) {
        placedBlock = world::wallVariants::of(placedBlock).value_or(placedBlock);
    }
    const std::optional<std::int32_t> defaultStateId = reg::blocks::defaultStateIdOf(placedBlock);
    if (!defaultStateId.has_value()) {
        return false;
    }

    // parity: `BlockPlaceContext`.
    const bool replaceClicked = clickedState.has_value() && reg::blockShapes::isReplaceable(*clickedState) &&
                                reg::blockStateDefinitions::blockOf(*clickedState) != nullptr &&
                                *reg::blockStateDefinitions::blockOf(*clickedState) != heldBlock;
    std::optional<std::array<std::int32_t, 3>> scaffoldTarget;
    if (heldBlock.path() == "scaffolding") {
        scaffoldTarget = world::scaffoldingPlacement::target(
            world.state(), world::netherPortals::minY(world), world::netherPortals::maxY(world), hit.x, hit.y, hit.z,
            hit.direction, session->yaw, session->sneaking, hit.inside);
    }
    BlockPos placeAt = hitResult.pos.offset(hit.direction);
    if (scaffoldTarget.has_value()) {
        placeAt = BlockPos{(*scaffoldTarget)[0], (*scaffoldTarget)[1], (*scaffoldTarget)[2]};
    } else if (replaceClicked) {
        placeAt = hitResult.pos;
    }
    const std::optional<std::int32_t> existing = world.state().blockState(placeAt.x, placeAt.y, placeAt.z);
    if (!existing.has_value() || !reg::blockShapes::isReplaceable(*existing)) {
        return false;
    }

    random::RandomSource random = random::RandomSource::legacy(seedOf(context.blocks));
    const std::int32_t placedStateId =
        world::blockPlacement::stateFor(placedBlock, *defaultStateId,
                                        world::blockPlacement::Placement{.face = hit.direction,
                                                                         .clickY = hit.clickY,
                                                                         .clickX = hit.clickX,
                                                                         .clickZ = hit.clickZ,
                                                                         .yaw = session->yaw,
                                                                         .pitch = session->pitch,
                                                                         .replacing = *existing,
                                                                         .world = &world.state(),
                                                                         .worldName = world.name(),
                                                                         .x = placeAt.x,
                                                                         .y = placeAt.y,
                                                                         .z = placeAt.z},
                                        random);
    const std::int32_t blockStateId = world::doubleBlocks::firstHalf(placedStateId);
    // parity: `BlockItem.canPlace`: a block never goes down inside somebody.
    if (world::blockPlacement::isObstructed(blockStateId, placeAt.x, placeAt.y, placeAt.z,
                                            buildersInTheWay(context.blocks, *session, world))) {
        return false;
    }
    // parity: `canSurvive` refuses a torch hung on nothing instead of placing it and dropping it later.
    if (!world::blockSurvival::isSupported(world.state(), placeAt.x, placeAt.y, placeAt.z, blockStateId)) {
        return false;
    }
    // parity: `setPlacedBy` on a door, bed or tall flower: one block asked for, two put down.
    const std::optional<world::SecondHalf> second =
        world::doubleBlocks::secondHalf(blockStateId, placeAt.x, placeAt.y, placeAt.z);
    if (second.has_value()) {
        const std::optional<std::int32_t> room = world.state().blockState(second->x, second->y, second->z);
        if (!room.has_value() || !reg::blockShapes::isReplaceable(*room)) {
            return false;
        }
    }
    // Asked once the block is decided to go down; a plugin veto makes the client put its prediction back.
    const Identifier* placedName = reg::blockStateDefinitions::blockOf(blockStateId);
    if (context.placeAllowed && !context.placeAllowed(*session, placeAt.x, placeAt.y, placeAt.z,
                                                      placedName == nullptr ? "" : placedName->toString())) {
        if (context.blocks.putBack) {
            context.blocks.putBack(*session, placeAt.x, placeAt.y, placeAt.z,
                                   world.state().blockState(placeAt.x, placeAt.y, placeAt.z).value_or(0));
        }
        // The client took the item out of its own hand when it predicted the placement.
        if (context.sendInventory) {
            context.sendInventory(*session);
        }
        return false;
    }
    if (!world.state().setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId)) {
        return false;
    }
    // parity: `getStateForPlacement`.
    std::int32_t settledStateId = blockStateId;
    {
        random::RandomSource joining = random::RandomSource::legacy(seedOf(context.blocks));
        const world::neighbourUpdates::Context shapes = contextOf(world, joining);
        settledStateId = world::neighbourUpdates::connect(shapes, placeAt.x, placeAt.y, placeAt.z, blockStateId)
                             .value_or(settledStateId);
        // parity: `BaseRailBlock.onPlace` into `RailState.place` bends the new rail and every rail it meets.
        const std::optional<std::int32_t> rail = world::rails::place(
            world.state(), world.name(), placeAt.x, placeAt.y, placeAt.z,
            [&context, &world, &placeAt, &settledStateId](const std::int32_t x, const std::int32_t y,
                                                          const std::int32_t z, const std::int32_t stateId) {
                if (x == placeAt.x && y == placeAt.y && z == placeAt.z) {
                    settledStateId = stateId;
                } else {
                    announceBlock(context.blocks, world.name(), x, y, z, stateId);
                }
            });
        static_cast<void>(rail);
        // parity: `RedStoneWireBlock.getStateForPlacement`, which draws the dust as it lands.
        settledStateId =
            world::redstoneWire::shapeOnPlacement(world.state(), placeAt.x, placeAt.y, placeAt.z, settledStateId)
                .value_or(settledStateId);
    }
    if (second.has_value()) {
        world.state().setBlockState(second->x, second->y, second->z, second->stateId);
    }
    // parity: `Level.setBlock` gives the block entity as the block lands.
    if (reg::blockEntityTypes::ofBlock(placedBlock) != nullptr) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
        if (chunk != nullptr) {
            const std::optional<world::commandBlocks::Mode> mode = world::commandBlocks::modeOf(placedBlock.path());
            wire::nbt::NbtCompound data = world::blockEntityDefaults::of(placedBlock);
            // parity: `CommandBlock.setPlacedBy`.
            if (mode.has_value()) {
                data.putBoolean("auto", *mode == world::commandBlocks::Mode::Repeating);
                data.putBoolean("TrackOutput", world.gameRules().get(world::gameRules::kSendCommandFeedback));
            }
            if (world::decoratedPots::isPot(placedBlock.path())) {
                world::decoratedPots::decorate(data, held.components);
            }
            // parity: `StructureBlock.setPlacedBy`, which signs it with whoever put it down.
            if (placedBlock.path() == "structure_block") {
                data.putString("author", player.name());
            }
            chunk->setBlockEntity(server::world::ChunkBlockEntity{placeAt.x & 15, placeAt.y, placeAt.z & 15,
                                                                  placedBlock, std::move(data)});
            if (mode.has_value()) {
                world::commandBlocks::onDataLoaded(world.state(), placeAt.x, placeAt.y, placeAt.z, false);
            }
        }
    }
    const Identifier* placedPath = reg::blockStateDefinitions::blockOf(blockStateId);
    // parity: `BlockItem.updateCustomBlockEntityTag`.
    if (placedPath != nullptr && world::shulkers::isShulkerBox(placedPath->path())) {
        const wire::nbt::NbtCompound* carried = held.components.getCompound(world::shulkers::kContainer);
        server::world::Chunk* chunk = world.state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
        server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(placeAt.x & 15, placeAt.y, placeAt.z & 15);
        if (carried != nullptr && entity != nullptr) {
            world::shulkers::restore(entity->data(), *carried);
            chunk->markDirty();
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), placeAt.x, placeAt.y,
                                                                 placeAt.z,
                                                                 world::comparatorReading::of(world.state()));
        }
    }
    // parity: `DiodeBlock.setPlacedBy`.
    world::redstoneComponents::bookOnPlacement(world.state(), world.name(), placeAt.x, placeAt.y, placeAt.z,
                                               world::comparatorReading::of(world.state()));
    // parity: `TripWireHookBlock.setPlacedBy` calls `calculateState`.
    if (placedPath != nullptr && placedPath->path() == "tripwire_hook") {
        std::vector<world::BlockChange> hookChanges;
        world::tripwires::calculateHook(world.state(), placeAt.x, placeAt.y, placeAt.z, world.gameTime(), hookChanges);
        for (const world::BlockChange& change : hookChanges) {
            announceBlock(context.blocks, world.name(), change.x, change.y, change.z, change.stateId);
        }
    }
    // parity: `MossyCarpetBlock.setPlacedBy`.
    if (placedPath != nullptr && placedPath->path() == "pale_moss_carpet") {
        const std::optional<std::int32_t> topper =
            world::mossCarpetTopper::above(world.state(), placeAt.x, placeAt.y, placeAt.z,
                                           [&context] { return context.nextBool ? context.nextBool() : false; });
        if (topper.has_value()) {
            world.state().setBlockState(placeAt.x, placeAt.y + 1, placeAt.z, *topper);
        }
    }

    if (session->gamemode == Gamemode::Survival) {
        session->inventory.set(heldSlot,
                               held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
        if (context.blocks.slotChanged) {
            context.blocks.slotChanged(*session, heldSlot);
        }
    }

    announceBlock(context.blocks, world.name(), placeAt.x, placeAt.y, placeAt.z, settledStateId);
    if (second.has_value()) {
        announceBlock(context.blocks, world.name(), second->x, second->y, second->z, second->stateId);
    }
    refreshSnowAt(context.blocks, world, placeAt.x, placeAt.y - 1, placeAt.z);
    refreshSnowAt(context.blocks, world, placeAt.x, placeAt.y, placeAt.z);
    notifyAround(context.blocks, world, placeAt.x, placeAt.y, placeAt.z, std::nullopt);
    if (context.placed) {
        context.placed(world, placeAt.x, placeAt.y, placeAt.z);
    }
    // parity: `BlockItem.place` grants off the final state.
    if (context.blocks.triggers != nullptr && context.blocks.lootTables != nullptr &&
        context.blocks.triggers->listensTo("placed_block")) {
        context.blocks.triggers->fire(
            "placed_block", player.name(), player.uuid().toString(),
            world::TriggerEvent{.location = world::triggerFacts::atBlock(*context.blocks.lootTables, world, player,
                                                                         placeAt.x, placeAt.y, placeAt.z, held)});
    }
    // parity: `ItemStack.useOn`, a block item that went down counts as used.
    statAwards::used(player, held.id);
    // parity: `GameEvent.BLOCK_PLACE`.
    heardBySculk(context.blocks, world, "block_place", placeAt.x, placeAt.y, placeAt.z, {.entity = player.uuid()});
    // parity: `SignBlock.setPlacedBy` opens the editor only on an unwaxed sign whose front a player could have typed.
    if (const std::optional<Identifier> signType = world::signs::typeOf(blockStateId); signType.has_value()) {
        const wire::nbt::NbtCompound data = world::signs::withComponents(world::signs::emptyData(), held.components);
        if (server::world::Chunk* chunk = world.state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
            chunk != nullptr) {
            chunk->setBlockEntity(
                world::signs::blockEntity(placeAt.x & 15, placeAt.y, placeAt.z & 15, *signType, data));
        }
        if (context.openSign && !world::signs::isWaxed(data) && world::signs::hasEditableText(data, true)) {
            context.openSign(*session, placeAt.x, placeAt.y, placeAt.z, blockStateId);
        }
    }
    return true;
}

} // namespace basalt::server::runtime::player
