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

namespace {

/// parity: `Entity.checkInsideBlocks`.
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
    // parity: `FallingBlockEntity`.
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
        // parity: `FrogspawnBlock.entityInside`.
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
            // parity: `FallingBlockEntity.tick`.
            broadcaster_->notifyAround(world, watchers, landed.placed->x, landed.placed->y, landed.placed->z,
                                       std::nullopt);
        } else if (landed.shattered.has_value()) {
            broadcaster_->dropFallenBlock(world, wasX, wasY, wasZ, *landed.shattered);
        }
    }
}

world::randomTicks::Context RedstoneTickSystem::randomContext(world::LoadedWorld& world) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::randomTicks::Context{
        .world = world.state(),
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
                return tags != nullptr && world::realBlockTags::contains(*tags, "minecraft:flowers", block);
            },
        .structureAllowed =
            [this, &world](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                return !hooks_.structureAllowed || hooks_.structureAllowed(world, x, y, z, stateId);
            },
        .levelEvent =
            [this, &world](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t type) {
                broadcaster_->sendEffects(watchersOf(world), x, y, z, world.state().blockState(x, y, z).value_or(0),
                                          {world::blockEffect::Effect{.type = type}});
            },
        // parity: `Block.pushEntitiesUp` reaches players too, which teleport to the top of the block.
        .pushPlayersUp =
            [this, &world](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t) {
                constexpr double kMudHeight = 14.0 / 16.0;
                constexpr double kHalfWidth = 0.3;
                constexpr double kHeight = 1.8;
                for (PlayerSession* player : watchersOf(world)) {
                    const bool overlaps = player->x + kHalfWidth > x && player->x - kHalfWidth < x + 1.0 &&
                                          player->z + kHalfWidth > z && player->z - kHalfWidth < z + 1.0 &&
                                          player->y + kHeight > y + kMudHeight && player->y < y + 1.0;
                    if (overlaps && player->connection().teleportTo) {
                        player->connection().teleportTo(player->x, y + 1.0, player->z, player->yaw, player->pitch);
                    }
                }
            }};
}

bool RedstoneTickSystem::randomTickAt(world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                                      const std::int32_t z) {
    const world::randomTicks::Context context = randomContext(world);
    random::RandomSource random = freshRandom();
    const std::vector<world::BlockChange> changes = randomTicks_.tickBlock(context, x, y, z, random);
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
        broadcaster_->notifyAround(world, watchers, change.x, change.y, change.z, std::nullopt);
    }
    return !changes.empty();
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
    const world::randomTicks::Context context = randomContext(world);
    random::RandomSource random = freshRandom();
    const std::vector<world::BlockChange> changes = randomTicks_.tick(context, chunks, speed, random);
    if (changes.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
        // parity: `setBlock`.
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
        .furnaceBurn = hooks_.furnaceBurn,
        .furnaceStarted = hooks_.furnaceStarted,
        .isBookshelfBook =
            [this](const wire::common::Identifier& item) {
                return options_.itemTags != nullptr &&
                       world::realItemTags::contains(*options_.itemTags, "minecraft:bookshelf_books", item);
            },
        .doesNotBlockHoppers =
            [this](const wire::common::Identifier& block) {
                return options_.blockTags != nullptr &&
                       world::realBlockTags::contains(*options_.blockTags, "minecraft:does_not_block_hoppers", block);
            },
        .songLength = [this](const std::string& song) -> std::int64_t {
            return options_.jukeboxes == nullptr ? 0 : options_.jukeboxes->lengthOf(song);
        },
        .isBeaconBase =
            [this](const wire::common::Identifier& block) {
                return options_.blockTags != nullptr &&
                       world::realBlockTags::contains(*options_.blockTags, "minecraft:beacon_base_blocks", block);
            },
        // parity: `BeaconBlockEntity.applyEffects`.
        .beaconPulse =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t levels,
                           const std::string& primary, const std::string& secondary) {
                world::beacons::Beam beam = world::beacons::beamOf(levels, primary == secondary);
                if (const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
                    const server::world::ChunkBlockEntity* held = chunk->blockEntityAt(x & 15, y, z & 15);
                    if (held != nullptr && held->data().contains("BeaconRange")) {
                        beam.range = held->data().getDouble("BeaconRange");
                    }
                }
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
        // parity: `BeaconBlockEntity.tick`.
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
        .creakingHeart =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z,
                           std::vector<world::BlockChange>& pending) {
                return options_.tickCreakingHeart && options_.tickCreakingHeart(world, x, y, z, pending);
            },
        .sculkCatalyst =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
                return options_.tickSculkCatalyst && options_.tickSculkCatalyst(world, x, y, z);
            },
        .conduit =
            [this, &world](wire::nbt::NbtCompound& data, const std::int32_t x, const std::int32_t y,
                           const std::int32_t z) {
                return options_.tickConduit && options_.tickConduit(world, x, y, z, data);
            },
        .gatewayBeam =
            [this, &world](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value() ||
                    (options_.triggerGatewayCooldown && !options_.triggerGatewayCooldown(world, x, y, z))) {
                    return;
                }
                for (PlayerSession* watcher : watchersOf(world)) {
                    const double dx = watcher->x - x;
                    const double dy = watcher->y - y;
                    const double dz = watcher->z - z;
                    if (watcher->worldName != world.name() || !watcher->connection().sendBlockEvent ||
                        (dx * dx) + (dy * dy) + (dz * dz) >= kBlockEventRange * kBlockEventRange) {
                        continue;
                    }
                    watcher->connection().sendBlockEvent(x, y, z, *state, world::endGateways::kBeamEvent, 0);
                }
            },
        .ticking = tickingChunks(world, region, 1),
        .spawnerTick = [this, &world](wire::nbt::NbtCompound& data, std::int32_t x, std::int32_t y,
                                      std::int32_t z) { return tickSpawner(world, data, x, y, z); }};
    random::RandomSource random = freshRandom();
    std::vector<world::BlockChange> changes = world::blockEntityTicks::tick(
        world.state(), world.name(), world.clock(), world.skyDarken(), world.gameTime(), chunks, random, hooks,
        config::dimensionFacts(world.declaration().dimension).hasSkylight,
        config::dimensionFacts(world.declaration().dimension).hasDayTimeline, world.metadata().seed);
    // parity: `PistonMovingBlockEntity`.
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
        // parity: `setBlock(..., 3)`.
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
