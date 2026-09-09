#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

namespace basalt::server::runtime {
namespace {

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
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(ClientboundBlockUpdatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            ClientboundBlockUpdatePacket{.pos = pos, .blockStateId = stateId}.write(buffer);
        });
    }
}

/// The tripwire block a thing at these coordinates is standing in, if it is standing in one.
void crossingsOf(world::LoadedWorld& world, double x, double y, double z, std::set<std::int64_t>& into) {
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    const auto from = static_cast<std::int32_t>(std::floor(y - 0.1));
    const auto to = static_cast<std::int32_t>(std::floor(y));
    for (std::int32_t blockY = from; blockY <= to; blockY++) {
        const std::optional<std::int32_t> stateId = world.state().blockState(blockX, blockY, blockZ);
        if (!stateId.has_value()) {
            continue;
        }
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId);
        if (block == nullptr || *block != world::tripwires::wireBlock()) {
            continue;
        }
        if (world::tripwires::isStandingOn(blockX, blockY, blockZ, x, y, z)) {
            into.insert(pack(blockX, blockY, blockZ));
        }
    }
}

} // namespace

RedstoneTickSystem::RedstoneTickSystem(player::PlayerRegistry& players, world::RealRecipes& recipes,
                                       BlockChangeBroadcaster& broadcaster, ChunkSimulationView& chunkSimulation,
                                       Hooks hooks, Options options)
    : players_(&players), recipes_(&recipes), broadcaster_(&broadcaster), chunkSimulation_(&chunkSimulation),
      hooks_(std::move(hooks)), options_(std::move(options)) {}

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

void RedstoneTickSystem::tickFluids(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<std::array<std::int32_t, 3>> due = world.state().dueFluidTicks();
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
                                                    world.declaration().dimension == config::WorldDimension::Nether};
        for (const world::FluidChange& change :
             world::fluidFlow::tick(context, position[0], position[1], position[2])) {
            const BlockPos pos{change.x, change.y, change.z};
            broadcaster_->dropWashedAwayBlock(world, change.x, change.y, change.z, change.washedAway);
            sendBlockUpdate(watchers, pos, change.stateId);
            // parity: `Level.setBlock` from the fluid tick updates the neighbours like any other
            // block change. Without it, water that washed a snow layer away left the grass under it
            // wearing its white top for ever, since nothing ever asked it to look again.
            broadcaster_->notifyAround(world, watchers, pos.x, pos.y, pos.z);
        }
    }
}

void RedstoneTickSystem::tickPressurePlates(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `entityInside` is the entity's doing, not the player's. An item lying on a wooden
    // plate presses it with nobody around, so this cannot start by asking whether anybody is here.
    const std::vector<world::DroppedItem> items = world.state().items();
    const auto touch = [this, &world, &region, &watchers, &items](double atX, double atY, double atZ,
                                                                  std::size_t movedThrough) {
        if (!region.ownsPosition(atX, atZ)) {
            return;
        }
        const auto x = static_cast<std::int32_t>(std::floor(atX));
        const auto z = static_cast<std::int32_t>(std::floor(atZ));
        const std::int32_t y = world::pressurePlates::blockUnder(atY);
        const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
        if (!stateId.has_value() || !world::pressurePlates::isPlate(*stateId) ||
            !world::pressurePlates::isStandingOn(x, y, z, atX, atY, atZ)) {
            return;
        }
        if (const std::optional<world::BlockChange> change =
                world::pressurePlates::update(world.state(), x, y, z, world::pressurePlates::Trigger::Touched,
                                              standingOn(world, x, y, z, watchers, items, movedThrough));
            change.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *change);
        }
    };
    for (const PlayerSession* player : watchers) {
        if (player->gamemode != Gamemode::Spectator) {
            touch(player->x, player->y, player->z, items.size());
        }
    }
    // parity: an item presses the plate on its own turn, so what it counts is itself and whoever
    // moved before it. The ones still to move are counted where they were when the tick began.
    for (std::size_t index = 0; index < items.size(); ++index) {
        touch(items[index].x, items[index].y, items[index].z, index + 1);
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
    // parity: `ButtonBlock.checkPressed` books the release, and books it again every turn an arrow
    // is still there.
    if (const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(*stateId);
        pressed.has_value()) {
        world.state().scheduleBlockTick(x, y, z, wire::common::Identifier::vanilla("button"), *pressed);
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
    for (const PlayerSession* player : watchers) {
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
        // parity: the warden's own `VibrationUser`, which hears a step sixteen blocks away and gets
        // angrier at whoever took it. Sneaking makes no vibration, so it hears nothing.
        if (!player->sneaking) {
            hearFootstep(world, *player);
        }
        if (!world.state().hasSculkSensorNear(player->x, player->y, player->z, world::vibrations::kRange)) {
            continue;
        }
        world::vibrations::emit(world.state(), "step", player->x, player->y, player->z, world.gameTime());
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
}

void RedstoneTickSystem::hearFootstep(world::LoadedWorld& world, const PlayerSession& player) {
    for (entity::ServerEntity* entity : world.state().entities()) {
        if (entity->removed || !entity::wardens::isWarden(entity->type())) {
            continue;
        }
        const double dx = entity->x - player.x;
        const double dy = entity->y - player.y;
        const double dz = entity->z - player.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) > entity::wardens::kHearingRange * entity::wardens::kHearingRange) {
            continue;
        }
        entity::wardens::anger(*entity, player.entityId(), entity::wardens::kHeard);
    }
}

void RedstoneTickSystem::tickTripwires(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    std::vector<world::BlockChange> changes;
    std::set<std::int64_t> crossed;
    for (const PlayerSession* player : watchers) {
        if (player->gamemode == Gamemode::Spectator || !region.ownsPosition(player->x, player->z)) {
            continue;
        }
        crossingsOf(world, player->x, player->y, player->z, crossed);
    }
    for (const world::DroppedItem& item : world.state().items()) {
        if (region.ownsPosition(item.x, item.z)) {
            crossingsOf(world, item.x, item.y, item.z, crossed);
        }
    }
    for (const std::int64_t packed : crossed) {
        world::tripwires::press(world.state(), unpackX(packed), unpackY(packed), unpackZ(packed), true, changes);
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
}

std::int32_t RedstoneTickSystem::standingOn(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            const std::vector<PlayerSession*>& watchers,
                                            const std::vector<world::DroppedItem>& items, std::size_t movedThrough) {
    std::int32_t count = 0;
    for (const PlayerSession* player : watchers) {
        if (player->gamemode != Gamemode::Spectator &&
            world::pressurePlates::isStandingOn(x, y, z, player->x, player->y, player->z)) {
            count++;
        }
    }
    // parity: the stone and blackstone plates are `MOBS`, so a dropped item does not press them.
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? server::registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    const std::string_view path = block == nullptr ? std::string_view{} : block->path();
    if (path != "stone_pressure_plate" && path != "polished_blackstone_pressure_plate") {
        for (std::size_t index = 0; index < items.size(); ++index) {
            const world::DroppedItem& item = items[index];
            const bool taken = index < movedThrough;
            if (world::pressurePlates::isStandingOn(x, y, z, taken ? item.x : item.previousX,
                                                    taken ? item.y : item.previousY, taken ? item.z : item.previousZ)) {
                count++;
            }
        }
    }
    return count;
}

void RedstoneTickSystem::tickFallingBlocks(world::LoadedWorld& world, const sched::Region& region) {
    std::vector<entity::ServerEntity*> falling;
    for (entity::ServerEntity* entity : world.state().entities()) {
        if (!entity->removed && entity->type() == world::fallingBlocks::entityType() &&
            region.ownsPosition(entity->x, entity->z)) {
            falling.push_back(entity);
        }
    }
    if (falling.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (entity::ServerEntity* entity : falling) {
        const std::optional<world::BlockChange> landed = world::fallingBlocks::tick(world.state(), *entity);
        if (landed.has_value()) {
            sendBlockUpdate(watchers, BlockPos{landed->x, landed->y, landed->z}, landed->stateId);
        }
    }
}

void RedstoneTickSystem::tickRandomBlocks(world::LoadedWorld& world, const sched::Region& region) {
    const std::int32_t speed = world.gameRules().number(world::gameRules::kRandomTickSpeed);
    if (speed <= 0) {
        return;
    }
    const std::vector<std::array<std::int32_t, 2>> chunks = chunkSimulation_->simulatedChunks(world, &region);
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
                                              .mayChange = hooks_.mayChange};
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
        broadcaster_->notifyAround(world, watchers, change.x, change.y, change.z);
    }
}

void RedstoneTickSystem::tickBlockEntities(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<std::array<std::int32_t, 2>> chunks = chunkSimulation_->simulatedChunks(world, &region);
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
        .songLength = [this](const std::string& song) -> std::int64_t {
            return options_.jukeboxes == nullptr ? 0 : options_.jukeboxes->lengthOf(song);
        },
        .songStopped =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
                broadcaster_->notifyAround(world, watchersOf(world), x, y, z);
            }};
    const std::vector<world::BlockChange> changes =
        world::blockEntityTicks::tick(world.state(), world.name(), world.clock(), world.weather(), chunks, hooks,
                                      config::dimensionFacts(world.declaration().dimension).hasSkylight);
    if (changes.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
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
