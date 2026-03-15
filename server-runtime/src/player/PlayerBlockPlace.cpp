#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
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

/// The same stack with one fewer in it.
[[nodiscard]] storage::StoredItemStack withCount(const storage::StoredItemStack& stack, const std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

/// What the world tells its neighbours with, built from where the block went down.
[[nodiscard]] world::neighbourUpdates::Context contextOf(world::LoadedWorld& world, random::RandomSource& random) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::neighbourUpdates::Context{
        .world = world.state(),
        .worldName = world.name(),
        .minY = facts.chunkHeight.minY(),
        .maxY = facts.chunkHeight.maxY(),
        .random = random,
        // parity: the `water_evaporates` attribute, which `WetSpongeBlock.onPlace` reads. Only the
        // Nether carries it: water stays put in the End, so `!hasSkylight` is the wrong test.
        .evaporates = world.declaration().dimension == config::WorldDimension::Nether,
        .hasSkylight = facts.hasSkylight,
        .ultrawarm = world.declaration().dimension == config::WorldDimension::Nether,
        // parity: `PotentSulfurBlock.updateShape` is `validBlockState`, which reads the water above
        // and the block below to say what the geyser is doing now.
        .geyser =
            [&world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t stateId) {
                return world::geysers::validState(world.state(), world.gameTime(), x, y, z, stateId);
            },
        .gameTime = world.gameTime(),
        .tntExplodes = world.gameRules().get(world::gameRules::kTntExplodes)};
}

/// parity: `BlockItem.canPlace`: a block never goes down inside somebody. Spectators are built
/// straight through, and the one placing counts even where the world has not listed them yet.
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
    // parity: `BlockItem` on a wall: a torch, a sign, a banner or a head clicked against a side goes
    // down as its wall form, not as the standing one refusing to stand.
    const bool againstAWall = hit.direction != play::Direction::Up && hit.direction != play::Direction::Down;
    // parity: `BlockItem.getBlock`, which is not the item's own name for redstone dust, string or
    // any seed.
    Identifier placedBlock = reg::itemBlocks::of(heldBlock);
    if (againstAWall) {
        placedBlock = world::wallVariants::of(placedBlock).value_or(placedBlock);
    }
    const std::optional<std::int32_t> defaultStateId = reg::blocks::defaultStateIdOf(placedBlock);
    if (!defaultStateId.has_value()) {
        return false;
    }

    // parity: `BlockPlaceContext`: the clicked block is replaced when it is one of the things that
    // give way (grass, a fern, a snow layer, fire, a fluid), and the block goes next to it otherwise.
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
    // The half a two-block one starts as, whatever the state table's first entry happens to be.
    const std::int32_t blockStateId = world::doubleBlocks::firstHalf(placedStateId);
    // parity: `BlockItem.canPlace`: a block never goes down inside somebody.
    if (world::blockPlacement::isObstructed(blockStateId, placeAt.x, placeAt.y, placeAt.z,
                                            buildersInTheWay(context.blocks, *session, world))) {
        return false;
    }
    // parity: `canSurvive`, which refuses a torch hung on nothing rather than letting it be placed
    // and drop on the next update.
    if (!world::blockSurvival::isSupported(world.state(), placeAt.x, placeAt.y, placeAt.z, blockStateId)) {
        return false;
    }
    // parity: `setPlacedBy` on a door, a bed or a tall flower: the client asks for one block and the
    // game puts down two.
    const std::optional<world::SecondHalf> second =
        world::doubleBlocks::secondHalf(blockStateId, placeAt.x, placeAt.y, placeAt.z);
    if (second.has_value()) {
        const std::optional<std::int32_t> room = world.state().blockState(second->x, second->y, second->z);
        if (!room.has_value() || !reg::blockShapes::isReplaceable(*room)) {
            return false;
        }
    }
    // Asked once the server has decided the block would go down, and the client is told to put its
    // own prediction back when a plugin says no.
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
    // parity: `getStateForPlacement` joins a fence, a wall or a staircase to what is already there,
    // and the announcement below has to carry the state it settled on.
    std::int32_t settledStateId = blockStateId;
    {
        random::RandomSource joining = random::RandomSource::legacy(seedOf(context.blocks));
        const world::neighbourUpdates::Context shapes = contextOf(world, joining);
        settledStateId = world::neighbourUpdates::connect(shapes, placeAt.x, placeAt.y, placeAt.z, blockStateId)
                             .value_or(settledStateId);
        // parity: `BaseRailBlock.onPlace` into `RailState.place`, which bends the new rail and every
        // rail it decided to meet.
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
    // parity: `Level.setBlock` gives a block its block entity as it lands. Every block that owns one
    // needs it: a bell without one is a bell whose body the client never draws.
    if (reg::blockEntityTypes::ofBlock(placedBlock) != nullptr) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(placeAt.x >> 4, placeAt.z >> 4);
        if (chunk != nullptr) {
            const std::optional<world::commandBlocks::Mode> mode = world::commandBlocks::modeOf(placedBlock.path());
            wire::nbt::NbtCompound data = world::blockEntityDefaults::of(placedBlock);
            // parity: `CommandBlock.setPlacedBy`, which is the placing half and not the command one:
            // a repeating block put down **by hand** comes out already automatic, and `TrackOutput`
            // starts as whatever `sendCommandFeedback` says. A `/setblock` runs none of this.
            if (mode.has_value()) {
                data.putBoolean("auto", *mode == world::commandBlocks::Mode::Repeating);
                data.putBoolean("TrackOutput", world.gameRules().get(world::gameRules::kSendCommandFeedback));
            }
            chunk->setBlockEntity(server::world::ChunkBlockEntity{placeAt.x & 15, placeAt.y, placeAt.z & 15,
                                                                  placedBlock, std::move(data)});
            if (mode.has_value()) {
                world::commandBlocks::onDataLoaded(world.state(), placeAt.x, placeAt.y, placeAt.z, false);
            }
        }
    }
    const Identifier* placedPath = reg::blockStateDefinitions::blockOf(blockStateId);
    // parity: `BlockItem.updateCustomBlockEntityTag`: a shulker box placed from an item that carries
    // contents gets them back, which is the other half of breaking one.
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
    // parity: `DiodeBlock.setPlacedBy`: a repeater or comparator that lands on a live signal books
    // its turn here and nowhere else.
    world::redstoneComponents::bookOnPlacement(world.state(), world.name(), placeAt.x, placeAt.y, placeAt.z,
                                               world::comparatorReading::of(world.state()));
    // parity: `TripWireHookBlock.setPlacedBy` calls `calculateState`, which is what attaches a hook
    // to a line of string that was already there.
    if (placedPath != nullptr && placedPath->path() == "tripwire_hook") {
        std::vector<world::BlockChange> hookChanges;
        world::tripwires::calculateHook(world.state(), placeAt.x, placeAt.y, placeAt.z, world.gameTime(), hookChanges);
        for (const world::BlockChange& change : hookChanges) {
            announceBlock(context.blocks, world.name(), change.x, change.y, change.z, change.stateId);
        }
    }
    // parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one placed.
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
    // Both the block under it (a snow layer put on grass) and the block itself (grass put under
    // snow) can change how they look.
    refreshSnowAt(context.blocks, world, placeAt.x, placeAt.y - 1, placeAt.z);
    refreshSnowAt(context.blocks, world, placeAt.x, placeAt.y, placeAt.z);
    notifyAround(context.blocks, world, placeAt.x, placeAt.y, placeAt.z, std::nullopt);
    if (context.placed) {
        context.placed(world, placeAt.x, placeAt.y, placeAt.z);
    }
    // parity: `GameEvent.BLOCK_PLACE`, thirteen: a block going down is louder than one coming up.
    heardBySculk(context.blocks, world, "block_place", placeAt.x, placeAt.y, placeAt.z, {.entity = player.uuid()});
    if (world::signs::isSign(blockStateId) && context.openSign) {
        context.openSign(*session, placeAt.x, placeAt.y, placeAt.z, blockStateId);
    }
    return true;
}

} // namespace basalt::server::runtime::player
