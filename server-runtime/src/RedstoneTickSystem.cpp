#include "RedstoneTickShared.hpp"

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
#include <basalt/server/runtime/world/EndGateways.hpp>
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
using namespace redstoneTick;

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

/// The chunks whose turns run this tick.
world::WorldState::Ticking RedstoneTickSystem::tickingChunks(const world::LoadedWorld& world,
                                                             const sched::Region& region,
                                                             const std::int32_t extraRadius) const {
    // Built on the first question: an idle world never builds the set (441 entries at full view distance).
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
        std::function<bool(std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t)>
            flowGate;
        if (hooks_.flowAllowed) {
            flowGate = [this, &world](std::int32_t fx, std::int32_t fy, std::int32_t fz, std::int32_t tx,
                                      std::int32_t ty,
                                      std::int32_t tz) { return hooks_.flowAllowed(world, fx, fy, fz, tx, ty, tz); };
        }
        const world::fluidFlow::Context context{.world = world.state(),
                                                .gameRules = world.gameRules(),
                                                .random = random,
                                                .ultrawarm =
                                                    world.declaration().dimension == config::WorldDimension::Nether,
                                                .blockTags = options_.blockTags,
                                                .flowAllowed = flowGate};
        for (const world::FluidChange& change :
             world::fluidFlow::tick(context, position[0], position[1], position[2])) {
            const BlockPos pos{change.x, change.y, change.z};
            broadcaster_->dropWashedAwayBlock(world, change.x, change.y, change.z, change.washedAway);
            sendBlockUpdate(watchers, pos, change.stateId);
            // parity: `Level.setBlock`.
            broadcaster_->notifyAround(world, watchers, pos.x, pos.y, pos.z,
                                       change.washedAway == 0 ? std::nullopt : std::optional{change.washedAway});
        }
    }
}

void RedstoneTickSystem::tickPressurePlates(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `entityInside`.
    const std::vector<world::DroppedItem> items = world.state().items();
    const auto touch = [this, &world, &region, &watchers, &items](double atX, double atY, double atZ, double halfWidth,
                                                                  double height, std::size_t movedThrough) {
        // `y` is a range: the jar's box test counts a plate the body reaches upwards too.
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
    // parity: `entityInside`.
    for (const entity::ServerEntity* standing : world.state().entities()) {
        if (!standing->removed && !entity::blockTriggers::ignored(standing->type())) {
            const auto [half, height] = sizeOf(standing->type());
            touch(standing->x, standing->y, standing->z, half, height, items.size());
        }
    }
    for (std::size_t index = 0; index < items.size(); ++index) {
        touch(items[index].x, items[index].y, items[index].z, itemHalf, itemHeight, index + 1);
    }

    // parity: `BigDripleafBlock.entityInside`.
    const auto stand = [this, &world, &region, &watchers](double atX, double atY, double atZ, bool onGround,
                                                          bool carefully = false) {
        if (!onGround || !region.ownsPosition(atX, atZ)) {
            return;
        }
        const auto x = static_cast<std::int32_t>(std::floor(atX));
        const auto y = static_cast<std::int32_t>(std::floor(atY));
        const auto z = static_cast<std::int32_t>(std::floor(atZ));
        // parity: `RedStoneOreBlock.stepOn`.
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
        // parity: `canEntityTilt`.
        stand(item.x, item.y, item.z, item.onGround);
    }
    // parity: `entityInside`.
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
        // parity: `entityInside`.
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
    // parity: `entityInside`.
    if (!world::arrowButtons::reaches(x, y, z, *stateId, arrowX, arrowY, arrowZ, half, height)) {
        return;
    }
    const std::optional<world::blockInteractions::BlockToggle> toggled =
        world::blockInteractions::toggle(world.state(), x, y, z);
    if (!toggled.has_value()) {
        return;
    }
    // parity: `checkPressed`.
    if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
        world::vibrations::emit(world.state(), "block_activate", x + 0.5, y + 0.5, z + 0.5, world.gameTime());
    }
    // parity: `ButtonBlock.checkPressed`.
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
        // parity: `RedStoneOreBlock.stepOn`.
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
            // parity: `Block.stepOn`.
            if (options_.steppedOn) {
                options_.steppedOn(world, *player, static_cast<std::int32_t>(std::floor(player->x)),
                                   static_cast<std::int32_t>(std::floor(player->y - kOnPosDepth)),
                                   static_cast<std::int32_t>(std::floor(player->z)));
            }
        }
        if (!world.state().hasSculkSensorNear(player->x, player->y, player->z, world::vibrations::kCalibratedRange)) {
            continue;
        }
        // parity: `SculkSensorBlock.stepOn`.
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
    // parity: `RedStoneOreBlock.stepOn` is `Block.stepOn`, which any entity on the ground fires.
    for (const entity::ServerEntity* walker : world.state().entities()) {
        if (walker->removed || !walker->onGround) {
            continue;
        }
        const std::int64_t block =
            pack(static_cast<std::int32_t>(std::floor(walker->x)), static_cast<std::int32_t>(std::floor(walker->y)),
                 static_cast<std::int32_t>(std::floor(walker->z)));
        const auto seen = lastFootstep_.find(walker->entityId());
        if (seen != lastFootstep_.end() && seen->second == block) {
            continue;
        }
        lastFootstep_[walker->entityId()] = block;
        if (const std::optional<world::BlockChange> lit = world::miscGrowth::lightRedstoneOre(
                world.state(), static_cast<std::int32_t>(std::floor(walker->x)),
                world::pressurePlates::blockUnder(walker->y), static_cast<std::int32_t>(std::floor(walker->z)));
            lit.has_value()) {
            changes.push_back(*lit);
        }
    }
    constexpr std::size_t kMaxRemembered = 4096;
    if (lastFootstep_.size() > kMaxRemembered) {
        lastFootstep_.clear();
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
    // parity: `TripWireBlock.entityInside`.
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
    // parity: `getEntityCount(level, TOUCH_AABB.move(pos), ...)`.
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
    // parity: `PressurePlateBlock.getSignalStrength`.
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const std::string_view path = block == nullptr ? std::string_view{} : block->path();
    const bool livingOnly = path == "stone_pressure_plate" || path == "polished_blackstone_pressure_plate";
    // Any entity presses a plate, not just players and items: a cow must press one.
    for (const entity::ServerEntity* standing : world.state().entities()) {
        // parity: `BasePressurePlateBlock.getEntityCount`.
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

} // namespace basalt::server::runtime
