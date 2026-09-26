#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/entity/BlockTriggers.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// parity: `Entity.getOnPosLegacy`, `getOnPos(0.2f)`: the square a fifth of a block under the feet is
/// the one an entity is standing on.
constexpr double kOnPosDepth = 0.2;

using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ClientboundBlockUpdatePacket;
using wire::protocol::play::Gamemode;

std::int64_t pack(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3ffffff) << 38) | ((static_cast<std::int64_t>(z) & 0x3ffffff) << 12) |
           (static_cast<std::int64_t>(y) & 0xfff);
}

std::int32_t unpackX(std::int64_t packed) { return static_cast<std::int32_t>(packed >> 38); }

std::int32_t unpackY(std::int64_t packed) { return static_cast<std::int32_t>((packed << 52) >> 52); }

std::int32_t unpackZ(std::int64_t packed) { return static_cast<std::int32_t>((packed << 26) >> 38); }

void sendBlockUpdate(const std::vector<PlayerSession*>& watchers, const BlockPos& pos, std::int32_t stateId) {
    for (PlayerSession* player : watchers) {
        // Through the shared one, which says it the way each client reads it: a lever, a lamp or a
        // piston told only in Java's packet leaves the other edition looking at the old block.
        player::sendBlockChangeTo(*player, pos.x, pos.y, pos.z, stateId);
    }
}

/// Every tripwire block an entity of this size is crossing.
///
/// parity: the wire collects with the bounds of its **own** shape, so the sweep is over the squares
/// the entity's box reaches and the test is box against box. A string one square over still fires
/// when a shoulder is in it.
void crossingsOf(world::LoadedWorld& world, double x, double y, double z, double halfWidth, double height,
                 std::set<std::int64_t>& into) {
    for (std::int32_t blockX = wire::common::mth::floor(x - halfWidth);
         blockX <= wire::common::mth::floor(x + halfWidth); ++blockX) {
        for (std::int32_t blockY = wire::common::mth::floor(y); blockY <= wire::common::mth::floor(y + height);
             ++blockY) {
            for (std::int32_t blockZ = wire::common::mth::floor(z - halfWidth);
                 blockZ <= wire::common::mth::floor(z + halfWidth); ++blockZ) {
                const std::optional<std::int32_t> stateId = world.state().blockState(blockX, blockY, blockZ);
                if (!stateId.has_value()) {
                    continue;
                }
                const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
                if (block == nullptr || *block != world::tripwires::wireBlock()) {
                    continue;
                }
                if (world::tripwires::crosses(blockX, blockY, blockZ, *stateId, x, y, z, halfWidth, height)) {
                    into.insert(pack(blockX, blockY, blockZ));
                }
            }
        }
    }
}

} // namespace

RedstoneTickSystem::RedstoneTickSystem(player::PlayerRegistry& players, world::RealRecipes& recipes,
                                       BlockChangeBroadcaster& broadcaster, ChunkSimulationView& chunkSimulation,
                                       world::commandBlocks::RunCommand runCommand, Hooks hooks, Options options)
    : players_(&players), recipes_(&recipes), broadcaster_(&broadcaster), chunkSimulation_(&chunkSimulation),
      hooks_(std::move(hooks)), options_(std::move(options)), runCommand_(std::move(runCommand)) {}

random::RandomSource RedstoneTickSystem::freshRandom() const {
    return random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
}

std::vector<PlayerSession*> RedstoneTickSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

/// The chunks whose turns run this tick. parity: `ChunkMap.onFullChunkStatusChange` gives a chunk its
/// `LevelTicks` container at block ticking and takes it away below that, so a chunk that is loaded
/// and not ticking is frozen. Ours are the ones around a player plus the forced ones, which is the
/// same set.
world::WorldState::Ticking RedstoneTickSystem::tickingChunks(const world::LoadedWorld& world,
                                                             const sched::Region& region,
                                                             const std::int32_t extraRadius) const {
    // Worked out on the first question and not before: `dueTicks` skips a chunk with nothing booked
    // before it asks, so an idle world never builds the set at all. With a full view distance it is
    // four hundred and forty one entries, twice a tick, for nothing.
    auto keys = std::make_shared<std::optional<std::unordered_set<std::int64_t>>>();
    return [this, keys, &world, &region, extraRadius](std::int32_t chunkX, std::int32_t chunkZ) {
        if (!keys->has_value()) {
            std::unordered_set<std::int64_t> built;
            for (const std::array<std::int32_t, 2>& position :
                 chunkSimulation_->simulatedChunks(world, &region, extraRadius)) {
                built.insert((static_cast<std::int64_t>(position[0]) << 32) | static_cast<std::uint32_t>(position[1]));
            }
            *keys = std::move(built);
        }
        return (*keys)->contains((static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ));
    };
}

void RedstoneTickSystem::tickFluids(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<std::array<std::int32_t, 3>> due = world.state().dueFluidTicks(tickingChunks(world, region, 1));
    if (due.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const std::array<std::int32_t, 3>& position : due) {
        if (!region.owns(position[0] >> 4, position[2] >> 4)) {
            continue;
        }
        random::RandomSource random = freshRandom();
        const world::fluidFlow::Context context{.world = world.state(),
                                                .gameRules = world.gameRules(),
                                                .random = random,
                                                .ultrawarm =
                                                    world.declaration().dimension == config::WorldDimension::Nether,
                                                .blockTags = options_.blockTags};
        for (const world::FluidChange& change :
             world::fluidFlow::tick(context, position[0], position[1], position[2])) {
            const BlockPos pos{change.x, change.y, change.z};
            broadcaster_->dropWashedAwayBlock(world, change.x, change.y, change.z, change.washedAway);
            sendBlockUpdate(watchers, pos, change.stateId);
            // parity: `Level.setBlock` from the fluid tick updates the neighbours like any other
            // block change. Without it, water that washed a snow layer away left the grass under it
            // wearing its white top for ever, since nothing ever asked it to look again.
            broadcaster_->notifyAround(world, watchers, pos.x, pos.y, pos.z,
                                       change.washedAway == 0 ? std::nullopt : std::optional{change.washedAway});
        }
    }
}

void RedstoneTickSystem::tickPressurePlates(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `entityInside` is the entity's doing, not the player's. An item lying on a wooden
    // plate presses it with nobody around, so this cannot start by asking whether anybody is here.
    const std::vector<world::DroppedItem> items = world.state().items();
    // parity: a plate is pressed by a **box**, so an entity reaches every square its own box crosses
    // and not only the one its navel is over. Same sweep the arrow buttons already do.
    const auto touch = [this, &world, &region, &watchers, &items](double atX, double atY, double atZ, double halfWidth,
                                                                  double height, std::size_t movedThrough) {
        // The `y` is a range and not the square under the feet: a body is taller than a block, and
        // the jar's box test counts a plate it reaches **upwards** too, which is how a plate on top
        // of a step presses for somebody standing beside it.
        for (std::int32_t x = wire::common::mth::floor(atX - halfWidth); x <= wire::common::mth::floor(atX + halfWidth);
             ++x) {
            for (std::int32_t y = wire::common::mth::floor(atY); y <= wire::common::mth::floor(atY + height); ++y) {
                for (std::int32_t z = wire::common::mth::floor(atZ - halfWidth);
                     z <= wire::common::mth::floor(atZ + halfWidth); ++z) {
                    if (!region.ownsPosition(static_cast<double>(x) + 0.5, static_cast<double>(z) + 0.5)) {
                        continue;
                    }
                    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                    if (!stateId.has_value() || !world::pressurePlates::isPlate(*stateId) ||
                        !world::pressurePlates::touches(x, y, z, atX, atY, atZ, halfWidth, height)) {
                        continue;
                    }
                    if (const std::optional<world::BlockChange> change = world::pressurePlates::update(
                            world.state(), x, y, z, world::pressurePlates::Trigger::Touched,
                            standingOn(world, x, y, z, watchers, items, movedThrough), world.gameTime());
                        change.has_value()) {
                        broadcaster_->announceRedstone(world, watchers, *change);
                    }
                }
            }
        }
    };
    const auto sizeOf = [](const wire::common::Identifier& type) {
        return std::pair{server::registry::entityTypeShapes::widthOf(type) / 2.0,
                         server::registry::entityTypeShapes::heightOf(type)};
    };
    const auto [playerHalf, playerHeight] = sizeOf(wire::common::Identifier::vanilla("player"));
    const auto [itemHalf, itemHeight] = sizeOf(wire::common::Identifier::vanilla("item"));
    for (const PlayerSession* player : watchers) {
        if (player->gamemode != Gamemode::Spectator) {
            touch(player->x, player->y, player->z, playerHalf, playerHeight, items.size());
        }
    }
    // parity: `entityInside` runs for whoever is inside the square, and a mob is inside it just the
    // same. Only players and items ever asked here.
    for (const entity::ServerEntity* standing : world.state().entities()) {
        if (!standing->removed && !entity::blockTriggers::ignored(standing->type())) {
            const auto [half, height] = sizeOf(standing->type());
            touch(standing->x, standing->y, standing->z, half, height, items.size());
        }
    }
    // parity: an item presses the plate on its own turn, so what it counts is itself and whoever
    // moved before it. The ones still to move are counted where they were when the tick began.
    for (std::size_t index = 0; index < items.size(); ++index) {
        touch(items[index].x, items[index].y, items[index].z, itemHalf, itemHeight, index + 1);
    }

    // parity: `BigDripleafBlock.entityInside`, which is the same question the plate asks and the
    // same sweep answers it: what is standing on the square, and is anything holding the leaf up.
    const auto stand = [this, &world, &region, &watchers](double atX, double atY, double atZ, bool onGround,
                                                          bool carefully = false) {
        if (!onGround || !region.ownsPosition(atX, atZ)) {
            return;
        }
        const auto x = static_cast<std::int32_t>(std::floor(atX));
        const auto y = static_cast<std::int32_t>(std::floor(atY));
        const auto z = static_cast<std::int32_t>(std::floor(atZ));
        // parity: `RedStoneOreBlock.stepOn` lights the ore under whoever walks on it, unless they are
        // treading carefully, which is vanilla's word for crouching. The square is `getOnPosLegacy`,
        // a fifth of a block under their feet. Measured in `pisar`.
        if (!carefully) {
            if (const std::optional<world::BlockChange> lit = world::miscGrowth::lightRedstoneOre(
                    world.state(), x, static_cast<std::int32_t>(std::floor(atY - kOnPosDepth)), z);
                lit.has_value()) {
                broadcaster_->announceRedstone(world, watchers, *lit);
            }
        }
        for (const std::int32_t at : {y, y - 1}) {
            if (const std::optional<world::BlockChange> tipping =
                    world::dripleaves::stoodOn(world.state(), world.name(), x, at, z, atY, onGround, world.gameTime());
                tipping.has_value()) {
                broadcaster_->announceRedstone(world, watchers, *tipping);
                return;
            }
        }
    };
    for (const PlayerSession* player : watchers) {
        if (player->gamemode != Gamemode::Spectator) {
            stand(player->x, player->y, player->z, player->onGround, player->sneaking);
        }
    }
    for (const world::DroppedItem& item : items) {
        // parity: `canEntityTilt` asks `onGround()`, and an item still sliding along the leaf is on
        // the ground: waiting for it to settle started the leaf's clock three ticks late.
        stand(item.x, item.y, item.z, item.onGround);
    }
    // parity: `entityInside` runs for **every** entity standing on the leaf, not only for a player
    // and a dropped item: a block of tnt that lands on one tips it over just the same. Measured in
    // `hojagrande`. An arrow is left out because the flag this build calls `onGround` means "stuck"
    // for one, which is not the `onGround()` vanilla asks about.
    for (const entity::ServerEntity* one : world.state().entities()) {
        if (one == nullptr || one->removed || entity::projectiles::kindOf(one->type()) != nullptr) {
            continue;
        }
        stand(one->x, one->y, one->z, one->onGround);
    }
}

void RedstoneTickSystem::tickArrowButtons(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const wire::common::Identifier arrowType = wire::common::Identifier::vanilla("arrow");
    const double half = server::registry::entityTypeShapes::widthOf(arrowType) / 2.0;
    const double height = server::registry::entityTypeShapes::heightOf(arrowType);
    for (const entity::ServerEntity* arrow : world.state().entities()) {
        if (arrow == nullptr || arrow->removed || arrow->type() != arrowType ||
            !region.ownsPosition(arrow->x, arrow->z)) {
            continue;
        }
        // parity: `entityInside` runs for every square the entity's box touches, and an arrow's box
        // straddles two of them more often than not.
        for (std::int32_t x = wire::common::mth::floor(arrow->x - half); x <= wire::common::mth::floor(arrow->x + half);
             ++x) {
            for (std::int32_t y = wire::common::mth::floor(arrow->y); y <= wire::common::mth::floor(arrow->y + height);
                 ++y) {
                for (std::int32_t z = wire::common::mth::floor(arrow->z - half);
                     z <= wire::common::mth::floor(arrow->z + half); ++z) {
                    pressArrowButton(world, watchers, x, y, z, arrow->x, arrow->y, arrow->z, half, height);
                }
            }
        }
    }
}

void RedstoneTickSystem::pressArrowButton(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                          std::int32_t x, std::int32_t y, std::int32_t z, double arrowX, double arrowY,
                                          double arrowZ, double half, double height) {
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (!stateId.has_value()) {
        return;
    }
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !world::arrowButtons::acceptsArrows(block->path())) {
        return;
    }
    // parity: `entityInside` gives up on a button that is already down; its own turn is what looks
    // again.
    if (!world::arrowButtons::reaches(x, y, z, *stateId, arrowX, arrowY, arrowZ, half, height)) {
        return;
    }
    const std::optional<world::blockInteractions::BlockToggle> toggled =
        world::blockInteractions::toggle(world.state(), x, y, z);
    if (!toggled.has_value()) {
        return;
    }
    // parity: `checkPressed` raises `BLOCK_ACTIVATE` from the arrow, the same event a player's
    // click raises.
    if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
        world::vibrations::emit(world.state(), "block_activate", x + 0.5, y + 0.5, z + 0.5, world.gameTime());
    }
    // parity: `ButtonBlock.checkPressed` books the release, and books it again every turn an arrow
    // is still there.
    // The turn goes under the button's own name, which is what the chunk saves.
    if (const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(*stateId);
        pressed.has_value()) {
        world.state().scheduleBlockTick(x, y, z, *block, *pressed);
    }
    for (const world::BlockChange& change : toggled->changes) {
        broadcaster_->announceRedstone(world, watchers, change);
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(change.stateId, change.x, change.y, change.z);
            wall.has_value()) {
            broadcaster_->announceRedstone(
                world, watchers,
                world::BlockChange{.x = (*wall)[0],
                                   .y = (*wall)[1],
                                   .z = (*wall)[2],
                                   .stateId =
                                       world.state().blockState((*wall)[0], (*wall)[1], (*wall)[2]).value_or(0)});
        }
    }
}

void RedstoneTickSystem::tickFootsteps(world::LoadedWorld& world) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    if (watchers.empty()) {
        return;
    }
    std::vector<world::BlockChange> changes;
    for (PlayerSession* player : watchers) {
        if (player->gamemode == Gamemode::Spectator || !player->onGround) {
            continue;
        }
        const std::int64_t block =
            pack(static_cast<std::int32_t>(std::floor(player->x)), static_cast<std::int32_t>(std::floor(player->y)),
                 static_cast<std::int32_t>(std::floor(player->z)));
        const auto seen = lastFootstep_.find(player->entityId());
        if (seen != lastFootstep_.end() && seen->second == block) {
            continue;
        }
        lastFootstep_[player->entityId()] = block;
        // parity: `RedStoneOreBlock.stepOn`, which lights the ore under whoever walks over it unless
        // they are sneaking. The random tick that puts it out again was already here; nothing ever
        // lit it.
        if (!player->sneaking) {
            const auto oreX = static_cast<std::int32_t>(std::floor(player->x));
            const auto oreZ = static_cast<std::int32_t>(std::floor(player->z));
            const std::int32_t oreY = world::pressurePlates::blockUnder(player->y);
            if (const std::optional<world::BlockChange> lit =
                    world::miscGrowth::lightRedstoneOre(world.state(), oreX, oreY, oreZ);
                lit.has_value()) {
                changes.push_back(*lit);
            }
        }
        // parity: sneaking makes no vibration, so nothing hears the step or feels it.
        if (!player->sneaking) {
            // parity: `Block.stepOn` for the blocks outside redstone, the sculk shrieker among them,
            // on `Entity.getOnPos`, a fifth of a block under the feet.
            if (options_.steppedOn) {
                options_.steppedOn(world, *player, static_cast<std::int32_t>(std::floor(player->x)),
                                   static_cast<std::int32_t>(std::floor(player->y - kOnPosDepth)),
                                   static_cast<std::int32_t>(std::floor(player->z)));
            }
        }
        if (!world.state().hasSculkSensorNear(player->x, player->y, player->z, world::vibrations::kCalibratedRange)) {
            continue;
        }
        // parity: `SculkSensorBlock.stepOn`, which puts the step straight into the sensor under the
        // feet without asking whether it already has one on the way. Standing on one is not the same
        // as walking past it, and only the second went through `handleGameEvent`.
        const auto stepX = static_cast<std::int32_t>(std::floor(player->x));
        const auto stepZ = static_cast<std::int32_t>(std::floor(player->z));
        const std::int32_t stepY = world::pressurePlates::blockUnder(player->y);
        const world::vibrations::Source source =
            world::triggerFacts::vibrationSourceOf(*player, options_.advancementTriggers);
        if (world::vibrations::stoodOn(world.state(), stepX, stepY, stepZ, player->x, player->y, player->z,
                                       world.gameTime(), source)) {
            continue;
        }
        world::vibrations::emit(world.state(), "step", player->x, player->y, player->z, world.gameTime(), source);
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
}

void RedstoneTickSystem::tickTripwires(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    std::vector<world::BlockChange> changes;
    std::set<std::int64_t> crossed;
    const auto sizeOf = [](const wire::common::Identifier& type) {
        return std::pair{server::registry::entityTypeShapes::widthOf(type) / 2.0,
                         server::registry::entityTypeShapes::heightOf(type)};
    };
    const auto [playerHalf, playerHeight] = sizeOf(wire::common::Identifier::vanilla("player"));
    const auto [itemHalf, itemHeight] = sizeOf(wire::common::Identifier::vanilla("item"));
    for (const PlayerSession* player : watchers) {
        if (player->gamemode == Gamemode::Spectator || !region.ownsPosition(player->x, player->z)) {
            continue;
        }
        crossingsOf(world, player->x, player->y, player->z, playerHalf, playerHeight, crossed);
    }
    for (const world::DroppedItem& item : world.state().items()) {
        if (region.ownsPosition(item.x, item.z)) {
            crossingsOf(world, item.x, item.y, item.z, itemHalf, itemHeight, crossed);
        }
    }
    // parity: `TripWireBlock.entityInside` runs for whatever is inside the square, and `checkPressed`
    // counts anything that is not `isIgnoringBlockTriggers`. A mob walking a string sets it off, and
    // only players and items ever asked here.
    for (const entity::ServerEntity* crossing : world.state().entities()) {
        if (crossing->removed || entity::blockTriggers::ignored(crossing->type()) ||
            !region.ownsPosition(crossing->x, crossing->z)) {
            continue;
        }
        const auto [half, height] = sizeOf(crossing->type());
        crossingsOf(world, crossing->x, crossing->y, crossing->z, half, height, crossed);
    }
    for (const std::int64_t packed : crossed) {
        world::tripwires::press(world.state(), unpackX(packed), unpackY(packed), unpackZ(packed), true,
                                world.gameTime(), changes);
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
}

std::int32_t RedstoneTickSystem::standingOn(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            const std::vector<PlayerSession*>& watchers,
                                            const std::vector<world::DroppedItem>& items, std::size_t movedThrough) {
    std::int32_t count = 0;
    // parity: `getEntityCount(level, TOUCH_AABB.move(pos), ...)` counts every entity whose **box**
    // crosses the plate's, so the same entity can be counted by two plates at once and a weighted
    // plate reads what is over its edge.
    const auto sizeOf = [](const wire::common::Identifier& type) {
        return std::pair{server::registry::entityTypeShapes::widthOf(type) / 2.0,
                         server::registry::entityTypeShapes::heightOf(type)};
    };
    const auto [playerHalf, playerHeight] = sizeOf(wire::common::Identifier::vanilla("player"));
    const auto [itemHalf, itemHeight] = sizeOf(wire::common::Identifier::vanilla("item"));
    for (const PlayerSession* player : watchers) {
        if (player->gamemode != Gamemode::Spectator &&
            world::pressurePlates::touches(x, y, z, player->x, player->y, player->z, playerHalf, playerHeight)) {
            count++;
        }
    }
    // parity: `PressurePlateBlock.getSignalStrength` picks the class it counts from the plate's own
    // sensitivity: `EVERYTHING` counts any entity and `MOBS` only the living ones. The stone and
    // blackstone plates are the `MOBS` pair.
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const std::string_view path = block == nullptr ? std::string_view{} : block->path();
    const bool livingOnly = path == "stone_pressure_plate" || path == "polished_blackstone_pressure_plate";
    // A mob, an arrow, a boat or a lit tnt is an entity like any other, and none of them was counted:
    // this sweep only knew about players and items, so a cow never pressed anything.
    for (const entity::ServerEntity* standing : world.state().entities()) {
        // parity: `BasePressurePlateBlock.getEntityCount` filters with
        // `EntitySelector.NO_SPECTATORS.and(e -> !e.isIgnoringBlockTriggers())` before it counts.
        if (standing->removed || entity::blockTriggers::ignored(standing->type()) ||
            (livingOnly && !server::registry::entityAttributes::isLiving(standing->type()))) {
            continue;
        }
        const auto [half, height] = sizeOf(standing->type());
        if (world::pressurePlates::touches(x, y, z, standing->x, standing->y, standing->z, half, height)) {
            count++;
        }
    }
    if (!livingOnly) {
        for (std::size_t index = 0; index < items.size(); ++index) {
            const world::DroppedItem& item = items[index];
            const bool taken = index < movedThrough;
            if (world::pressurePlates::touches(x, y, z, taken ? item.x : item.previousX,
                                               taken ? item.y : item.previousY, taken ? item.z : item.previousZ,
                                               itemHalf, itemHeight)) {
                count++;
            }
        }
    }
    return count;
}

namespace {

/// parity: `Entity.checkInsideBlocks`, which walks the squares the box overlaps after deflating it
/// by a ten-millionth so a box resting exactly on an edge is not inside the block beyond it.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> squaresInside(double x, double y, double z, double half,
                                                                     double height) {
    constexpr double kEdgeSlack = 1.0e-7;
    std::vector<std::array<std::int32_t, 3>> out;
    for (std::int32_t bx = wire::common::mth::floor(x - half + kEdgeSlack);
         bx <= wire::common::mth::floor(x + half - kEdgeSlack); ++bx) {
        for (std::int32_t by = wire::common::mth::floor(y + kEdgeSlack);
             by <= wire::common::mth::floor(y + height - kEdgeSlack); ++by) {
            for (std::int32_t bz = wire::common::mth::floor(z - half + kEdgeSlack);
                 bz <= wire::common::mth::floor(z + half - kEdgeSlack); ++bz) {
                out.push_back({bx, by, bz});
            }
        }
    }
    return out;
}

} // namespace

void RedstoneTickSystem::tickFallingBlocks(world::LoadedWorld& world, const sched::Region& region) {
    // parity: a falling block is a `FallingBlockEntity` and takes its turn off `entityTickList`, so
    // it stops where it is when its chunk falls out of `ENTITY_TICKING`. This one does not go through
    // the world's entity loop, so it needs the same gate of its own.
    const world::WorldState::Ticking ticking = tickingChunks(world, region, 0);
    std::vector<entity::ServerEntity*> falling;
    for (entity::ServerEntity* entity : world.state().entities()) {
        if (!entity->removed && entity->type() == world::fallingBlocks::entityType() &&
            region.ownsPosition(entity->x, entity->z) &&
            ticking(static_cast<std::int32_t>(std::floor(entity->x)) >> 4,
                    static_cast<std::int32_t>(std::floor(entity->z)) >> 4)) {
            falling.push_back(entity);
        }
    }
    if (falling.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const bool hasSkylight = config::dimensionFacts(world.declaration().dimension).hasSkylight;
    const double half = server::registry::entityTypeShapes::widthOf(world::fallingBlocks::entityType()) / 2.0;
    const double height = server::registry::entityTypeShapes::heightOf(world::fallingBlocks::entityType());
    for (entity::ServerEntity* entity : falling) {
        const double wasX = entity->x;
        const double wasY = entity->y;
        const double wasZ = entity->z;
        const world::fallingBlocks::Landing landed = world::fallingBlocks::tick(
            world.state(), *entity,
            [&world, hasSkylight](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
                return world::blockSurvival::isSupported(world.state(), x, y, z, stateId, {}, hasSkylight);
            },
            [this, &world, &watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
                sendBlockUpdate(watchers, BlockPos{bx, by, bz}, stateId);
                broadcaster_->notifyAround(world, watchers, bx, by, bz, std::nullopt);
            });
        // parity: `FrogspawnBlock.entityInside` is `destroyBlock(pos, false)` for a falling block and
        // nothing else, and `checkInsideBlocks` runs inside the move, over the squares the box ends
        // up touching: sand dropped through a nest of eggs takes it with it and drops nothing.
        for (const std::array<std::int32_t, 3>& at : squaresInside(entity->x, entity->y, entity->z, half, height)) {
            const std::optional<std::int32_t> here = world.state().blockState(at[0], at[1], at[2]);
            if (!here.has_value() || server::registry::blockStateDefinitions::blockOf(*here) == nullptr ||
                server::registry::blockStateDefinitions::blockOf(*here)->path() != "frogspawn") {
                continue;
            }
            if (world.state().setBlockState(at[0], at[1], at[2], server::registry::blocks::air().stateId)) {
                sendBlockUpdate(watchers, BlockPos{at[0], at[1], at[2]}, server::registry::blocks::air().stateId);
                broadcaster_->notifyAround(world, watchers, at[0], at[1], at[2], *here);
            }
        }
        if (landed.placed.has_value()) {
            sendBlockUpdate(watchers, BlockPos{landed.placed->x, landed.placed->y, landed.placed->z},
                            landed.placed->stateId);
            // parity: `FallingBlockEntity.tick` lands with `setBlock(pos, state, 3)`, so the square
            // it stops in tells its neighbours and reshapes them. Writing the block and telling only
            // the client left a fence beside the landing square unjoined and a tilled soil under it
            // untouched, which is what `arenacae` measured.
            broadcaster_->notifyAround(world, watchers, landed.placed->x, landed.placed->y, landed.placed->z,
                                       std::nullopt);
        } else if (landed.shattered.has_value()) {
            broadcaster_->dropFallenBlock(world, wasX, wasY, wasZ, *landed.shattered);
        }
    }
}

void RedstoneTickSystem::tickRandomBlocks(world::LoadedWorld& world, const sched::Region& region) {
    const std::int32_t speed = world.gameRules().number(world::gameRules::kRandomTickSpeed);
    if (speed <= 0) {
        return;
    }
    const std::vector<std::array<std::int32_t, 2>> chunks = chunkSimulation_->simulatedChunks(world, &region, 1);
    if (chunks.empty()) {
        return;
    }
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::randomTicks::Context context{.world = world.state(),
                                              .worldName = world.name(),
                                              .seed = world.metadata().seed,
                                              .maxY = facts.chunkHeight.maxY(),
                                              .gameRules = world.gameRules(),
                                              .clock = world.clock(),
                                              .difficulty = world.difficulty(),
                                              .hasSkylight = facts.hasSkylight,
                                              .raining = world.weather().raining,
                                              .evaporates = !facts.hasSkylight,
                                              .spawnsPortalPiglins = facts.spawnsPortalPiglins,
                                              .allocateEntityId = [this] { return players_->allocateEntityId(); },
                                              .mayChange = hooks_.mayChange,
                                              .flowering =
                                                  [tags = options_.blockTags](const wire::common::Identifier& block) {
                                                      return tags != nullptr && world::realBlockTags::contains(
                                                                                    *tags, "minecraft:flowers", block);
                                                  }};
    random::RandomSource random = freshRandom();
    const std::vector<world::BlockChange> changes = randomTicks_.tick(context, chunks, speed, random);
    if (changes.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const world::BlockChange& change : changes) {
        // parity: a block entity that changed its own state is a block change like any other, so its
        // neighbours hear about it: a daylight detector that dropped to zero at dusk has to take the
        // dust beside it down with it.
        broadcaster_->announceRedstone(world, watchers, change);
        // parity: `setBlock` again: what a random tick changed reshapes what is around it, which is
        // how the grass under a snow layer that melted stops wearing snow.
        broadcaster_->notifyAround(world, watchers, change.x, change.y, change.z, std::nullopt);
    }
}

void RedstoneTickSystem::tickBlockEntities(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<std::array<std::int32_t, 2>> chunks = chunkSimulation_->simulatedChunks(world, &region, 1);
    if (chunks.empty()) {
        return;
    }
    const world::blockEntityTicks::Hooks hooks{
        .recipeFor =
            [this](server::world::crafting::Cooker cooker, const server::world::loot::ItemStack& stack) {
                return recipes_->cookingFor(cooker, stack);
            },
        // parity: `CampfireBlockEntity.cookTick`, which drops what is done beside the fire.
        .onDropped =
            [this, &world](const world::blockEntityTicks::Dropped& done) {
                world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                           .uuid = wire::common::Uuid::random(),
                                                           .item = done.stack.item,
                                                           .count = done.stack.count,
                                                           .x = done.x + 0.5,
                                                           .y = done.y + 1.0,
                                                           .z = done.z + 0.5});
            },
        .allowHopperMove = hooks_.allowHopperMove,
        .allowSmelt = hooks_.allowSmelt,
        .isBookshelfBook =
            [this](const wire::common::Identifier& item) {
                return options_.itemTags != nullptr &&
                       world::realItemTags::contains(*options_.itemTags, "minecraft:bookshelf_books", item);
            },
        .songLength = [this](const std::string& song) -> std::int64_t {
            return options_.jukeboxes == nullptr ? 0 : options_.jukeboxes->lengthOf(song);
        },
        .isBeaconBase =
            [this](const wire::common::Identifier& block) {
                return options_.blockTags != nullptr &&
                       world::realBlockTags::contains(*options_.blockTags, "minecraft:beacon_base_blocks", block);
            },
        // parity: `BeaconBlockEntity.applyEffects`: everybody inside the square it reaches, at any
        // height, gets the effect again for as long as the pyramid is worth.
        .beaconPulse =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t levels,
                           const std::string& primary, const std::string& secondary) {
                const world::beacons::Beam beam = world::beacons::beamOf(levels, primary == secondary);
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> first =
                    wire::common::Identifier::parse(primary);
                if (!first.has_value()) {
                    return;
                }
                const std::optional<wire::common::Identifier> second =
                    secondary.empty() || secondary == primary
                        ? std::optional<wire::common::Identifier>{}
                        : [&secondary]() -> std::optional<wire::common::Identifier> {
                    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                        wire::common::Identifier::parse(secondary);
                    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
                }();
                for (PlayerSession* player : watchersOf(world)) {
                    if (!world::beacons::reaches(beam, x, y, z, player->x, player->y, player->z) ||
                        !player->connection().addEffect) {
                        continue;
                    }
                    player->connection().addEffect(*first, beam.durationTicks, beam.amplifier);
                    if (levels >= world::beacons::kMaxLevels && second.has_value()) {
                        player->connection().addEffect(*second, beam.durationTicks, 0);
                    }
                }
            },
        // parity: `BeaconBlockEntity.tick` hands the criterion to every player in a box ten blocks
        // out and five up from the beacon, four deep under it.
        .beaconLit =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           const std::int32_t levels) {
                world::AdvancementTriggers* triggers = options_.advancementTriggers;
                if (triggers == nullptr) {
                    return;
                }
                constexpr double kReach = 10.0;
                constexpr double kUp = 5.0;
                constexpr double kUnder = 4.0;
                for (const player::PlayerSession* watcher : players_->snapshot()) {
                    if (watcher->worldName != world.name()) {
                        continue;
                    }
                    if (std::abs(watcher->x - x) > kReach || std::abs(watcher->z - z) > kReach ||
                        watcher->y > y + kUp || watcher->y < y - kUnder - kUp) {
                        continue;
                    }
                    triggers->fire("construct_beacon", watcher->name(), watcher->uuid().toString(),
                                   world::TriggerEvent{.numbers = {{"level", levels}}});
                }
            },
        .songStopped =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
                const std::vector<PlayerSession*> watchers = watchersOf(world);
                // parity: `JukeboxSongPlayer.stop`, which tells the clients before the neighbours.
                broadcaster_->sendEffects(
                    watchers, x, y, z, world.state().blockState(x, y, z).value_or(0),
                    {world::blockEffect::Effect{.type = world::blockEffect::Effect::kStopJukeboxSong}});
                broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
            },
        .shriekerHeard =
            [this, &world](const world::vibrations::ShriekerArrival& arrival) {
                if (options_.shriekerHeard) {
                    options_.shriekerHeard(world, arrival);
                }
            },
        .beehive =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           std::vector<world::BlockChange>& pending) {
                return options_.tickBeehive && options_.tickBeehive(world, x, y, z, pending);
            },
        .ticking = tickingChunks(world, region, 1)};
    random::RandomSource random = freshRandom();
    std::vector<world::BlockChange> changes = world::blockEntityTicks::tick(
        world.state(), world.name(), world.clock(), world.skyDarken(), world.gameTime(), chunks, random, hooks,
        config::dimensionFacts(world.declaration().dimension).hasSkylight,
        config::dimensionFacts(world.declaration().dimension).hasDayTimeline, world.metadata().seed);
    // parity: `PistonMovingBlockEntity` is a block entity like any other and ticks here, which is
    // why the jar books no turn while a piston slides. Every sliding square moves its own progress
    // along and lands itself.
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const std::array<std::int32_t, 2>& position : chunks) {
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(position[0], position[1]);
        if (chunk == nullptr || chunk->blockEntityCount() == 0) {
            continue;
        }
        for (const server::world::ChunkBlockEntity& sliding : chunk->blockEntities()) {
            if (sliding.type() != world::pistons::block()) {
                continue;
            }
            const std::array<std::int32_t, 3> at{(position[0] << 4) + sliding.localX(), sliding.y(),
                                                 (position[1] << 4) + sliding.localZ()};
            const std::vector<world::BlockChange> landed = broadcaster_->finishPiston(world, at, watchers);
            changes.insert(changes.end(), landed.begin(), landed.end());
        }
    }
    world.state().clearSlidesStarted();
    if (changes.empty()) {
        return;
    }
    for (const world::BlockChange& change : changes) {
        // parity: a block entity that rewrites its own block does it with `setBlock(..., 3)`, which
        // is `UPDATE_NEIGHBORS | UPDATE_CLIENTS`. Telling only the clients left the dust beside a
        // daylight detector reading nothing while the detector itself said eleven, which is what
        // `detectorluz` measured; the same held for anything else that changes on its own turn.
        broadcaster_->announceRedstone(world, watchers, change);
    }
}

void RedstoneTickSystem::tickInhabitedTime(world::LoadedWorld& world, const sched::Region& region) {
    for (const std::array<std::int32_t, 2>& position : chunkSimulation_->simulatedChunks(world, &region)) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(position[0], position[1]);
        if (chunk != nullptr) {
            chunk->increaseInhabitedTime();
        }
    }
}

} // namespace basalt::server::runtime
