#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/RedstoneTickSystem.hpp>
#include <basalt/server/runtime/entity/BlockTriggers.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/ArrowButtons.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Lecterns.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/SculkShriekers.hpp>
#include <basalt/server/runtime/world/SculkSpreading.hpp>
#include <basalt/server/runtime/world/StalactiteDrips.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ClientboundBlockUpdatePacket;
using wire::protocol::play::SoundPacket;
using wire::protocol::play::SoundSource;

/// parity: `ServerLevel.canSpreadFireAround`.
[[nodiscard]] bool canSpreadFireAround(const world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                       std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t radius = world.gameRules().number(world::gameRules::kFireSpreadRadiusAroundPlayer);
    if (radius == -1) {
        return true;
    }
    for (const PlayerSession* player : watchers) {
        if (player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) < static_cast<double>(radius)) {
            return true;
        }
    }
    return false;
}

void sendBlockUpdates(const std::vector<PlayerSession*>& watchers, const std::vector<world::BlockChange>& changes) {
    for (const world::BlockChange& change : changes) {
        for (PlayerSession* player : watchers) {
            // Goes through the shared sender: water or fire changes were reaching only one edition.
            player::sendBlockChangeTo(*player, change.x, change.y, change.z, change.stateId);
        }
    }
}

} // namespace

void RedstoneTickSystem::openTurn(world::LoadedWorld& world, const sched::Region& region) {
    world.state().openTurn(tickingChunks(world, region, 1));
}

void RedstoneTickSystem::releaseHatchlings(world::LoadedWorld& world,
                                           std::vector<world::scheduledBlocks::Hatchling>& hatched) {
    for (const world::scheduledBlocks::Hatchling& one : hatched) {
        if (options_.spawnHatchling) {
            options_.spawnHatchling(world, one);
        }
    }
    hatched.clear();
}

void RedstoneTickSystem::crafterCrafted(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        const server::world::crafting::CraftingInput& input,
                                        const world::blockInventory::Slots& grid) {
    world::AdvancementTriggers* triggers = options_.advancementTriggers;
    if (triggers == nullptr || recipes_ == nullptr || !triggers->listensTo("crafter_recipe_crafted")) {
        return;
    }
    const std::optional<world::RealRecipes::Match> match = recipes_->recipeMatching(input);
    if (!match.has_value()) {
        return;
    }
    std::vector<world::TriggerItem> ingredients;
    for (const std::optional<server::world::loot::ItemStack>& slot : grid) {
        ingredients.push_back(
            slot.has_value()
                ? world::TriggerItem{.item = slot->item, .count = slot->count, .components = slot->components}
                : world::TriggerItem{.item = wire::common::Identifier::vanilla("air"), .count = 0});
    }
    constexpr double kHalfBox = 8.5;
    for (PlayerSession* player : watchersOf(world)) {
        if (std::abs(player->x - (x + 0.5)) < kHalfBox && std::abs(player->y - (y + 0.5)) < kHalfBox &&
            std::abs(player->z - (z + 0.5)) < kHalfBox) {
            triggers->fire("crafter_recipe_crafted", player->name(), player->uuid().toString(),
                           world::TriggerEvent{.ids = {{"recipe_id", match->id}}, .ingredients = ingredients});
        }
    }
}

void RedstoneTickSystem::tickScheduledBlocks(world::LoadedWorld& world, const sched::Region& region) {
    const std::vector<world::WorldState::DueTick> due =
        world.state().dueBlockTicksTyped(tickingChunks(world, region, 1));
    if (due.empty()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const bool hasSkylight = config::dimensionFacts(world.declaration().dimension).hasSkylight;
    const world::redstoneComponents::ContainerSignalAt containerSignal = world::comparatorReading::of(world.state());
    for (const world::WorldState::DueTick& tick : due) {
        const std::array<std::int32_t, 3> position{tick.x, tick.y, tick.z};
        const std::int32_t x = position[0];
        const std::int32_t y = position[1];
        const std::int32_t z = position[2];
        // parity: `LevelTicks.runCollectedTicks`.
        world.state().beginBlockTick(x, y, z);
        if (!region.owns(x >> 4, z >> 4)) {
            continue;
        }
        const std::int32_t here = world.state().blockState(x, y, z).value_or(0);
        if (world::fires::isFire(here)) {
            if (hooks_.igniteAllowed && !hooks_.igniteAllowed(world, x, y, z, "spread")) {
                continue;
            }
            random::RandomSource random = freshRandom();
            const world::fires::Surroundings surroundings{
                .rainingAt = [&world](std::int32_t rx, std::int32_t ry,
                                      std::int32_t rz) { return world::precipitation::rainingAt(world, rx, ry, rz); },
                .increasedBurnout = world::precipitation::increasedFireBurnout(world, x, y, z),
                .canSpread = canSpreadFireAround(world, watchers, x, y, z),
                .bedrockBurnsForever = world.declaration().dimension == config::WorldDimension::End};
            sendBlockUpdates(watchers, world::fires::tick(world.state(), world.weather(), world.difficulty(), x, y, z,
                                                          random, {}, hasSkylight, surroundings));
            continue;
        }
        if (world::fallingBlocks::falls(here)) {
            const entity::ServerEntity* fell = world::fallingBlocks::beginFalling(
                world.state(), x, y, z, [this] { return players_->allocateEntityId(); });
            if (fell != nullptr) {
                sendBlockUpdates(
                    watchers,
                    {world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId}});
            }
            continue;
        }
        std::vector<world::BlockChange> scheduled;
        std::vector<world::scheduledBlocks::Hatchling> hatched;
        random::RandomSource random = freshRandom();
        // parity: `LeavesBlock.tick`.
        if (const std::optional<world::BlockChange> leaves = world::neighbourUpdates::leafTick(world.state(), x, y, z);
            leaves.has_value()) {
            // parity: `LeavesBlock.tick`.
            sendBlockUpdates(watchers, {*leaves});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        world::scheduledBlocks::frostedIce(world.state(), x, y, z, random, scheduled);
        world::scheduledBlocks::driedGhast(world.state(), x, y, z, world.gameTime(), scheduled, hatched);
        releaseHatchlings(world, hatched);
        // parity: `EyeblossomBlock.tick`.
        if (world::miscGrowth::isEyeblossom(here)) {
            world::miscGrowth::eyeblossom(world.state(), world.clock(), x, y, z, here, random, scheduled);
        }
        // parity: `RedstoneLampBlock.tick`, the four ticks a lamp waits before going dark.
        if (const std::optional<world::BlockChange> lamp =
                world::redstoneReactors::lampTick(world.state(), world.name(), x, y, z);
            lamp.has_value()) {
            scheduled.push_back(*lamp);
        }
        // parity: `BasePressurePlateBlock.tick`, the turn a pressed plate books to come back up.
        if (world::pressurePlates::isPlate(here)) {
            if (const std::optional<world::BlockChange> plate =
                    world::pressurePlates::update(world.state(), x, y, z, world::pressurePlates::Trigger::Scheduled,
                                                  standingOn(world, x, y, z, watchers, world.state().items(),
                                                             std::numeric_limits<std::size_t>::max()),
                                                  world.gameTime());
                plate.has_value()) {
                scheduled.push_back(*plate);
            }
        }
        // parity: `TripWireBlock.tick`, the recheck a pressed wire books for itself.
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(here);
        if (block != nullptr && *block == world::tripwires::wireBlock()) {
            bool stillOn = false;
            // parity: `state.getShape(...).bounds()`.
            const auto sizeOf = [](const wire::common::Identifier& type) {
                return std::pair{server::registry::entityTypeShapes::widthOf(type) / 2.0,
                                 server::registry::entityTypeShapes::heightOf(type)};
            };
            const auto [playerHalf, playerHeight] = sizeOf(wire::common::Identifier::vanilla("player"));
            const auto [itemHalf, itemHeight] = sizeOf(wire::common::Identifier::vanilla("item"));
            for (const PlayerSession* player : watchers) {
                if (world::tripwires::crosses(x, y, z, here, player->x, player->y, player->z, playerHalf,
                                              playerHeight)) {
                    stillOn = true;
                    break;
                }
            }
            if (!stillOn) {
                for (const world::DroppedItem& item : world.state().items()) {
                    if (world::tripwires::crosses(x, y, z, here, item.x, item.y, item.z, itemHalf, itemHeight)) {
                        stillOn = true;
                        break;
                    }
                }
            }
            // parity: `TripWireBlock.checkPressed`.
            if (!stillOn) {
                for (const entity::ServerEntity* standing : world.state().entities()) {
                    if (standing->removed || entity::blockTriggers::ignored(standing->type())) {
                        continue;
                    }
                    const auto [half, height] = sizeOf(standing->type());
                    if (world::tripwires::crosses(x, y, z, here, standing->x, standing->y, standing->z, half, height)) {
                        stillOn = true;
                        break;
                    }
                }
            }
            // `checkPressed` ends in `setBlock(..., 3)`, so releasing must settle the dust; packets left a lamp lit.
            std::vector<world::BlockChange> pressed;
            world::tripwires::press(world.state(), x, y, z, stillOn, world.gameTime(), pressed);
            for (const world::BlockChange& moved : pressed) {
                broadcaster_->announceRedstone(world, watchers, moved);
            }
        }
        // parity: `TripWireHookBlock.tick`.
        if (here != 0 && world::tripwires::isHook(here)) {
            std::vector<world::BlockChange> hook;
            world::tripwires::calculateHook(world.state(), x, y, z, world.gameTime(), hook);
            for (const world::BlockChange& moved : hook) {
                broadcaster_->announceRedstone(world, watchers, moved);
            }
        }
        // parity: `ComposterBlock.tick`.
        if (std::optional<world::BlockChange> ready = world::composters::settle(world.state(), x, y, z);
            ready.has_value()) {
            ready->effects.push_back(world::blockEffect::Sound{
                .name = "block.composter.ready", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F});
            broadcaster_->announceBlock(watchers, *ready);
            broadcaster_->sendEffects(watchers, x, y, z, ready->stateId, ready->effects);
            // parity: `setBlock(..., 3)`.
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z, containerSignal);
        }
        // parity: `LecternBlock.tick`, the two ticks a page turn pulses for.
        if (const std::optional<world::BlockChange> lectern = world::lecterns::endPulse(world.state(), x, y, z);
            lectern.has_value()) {
            scheduled.push_back(*lectern);
        }
        // parity: `DetectorRailBlock.tick`, the recheck a pressed rail books for itself.
        if (const world::detectorRails::Checked rail = world::detectorRails::check(
                world.state(), x, y, z, world.state().entities(), world::detectorRails::Caller::ScheduledTick);
            rail.ran) {
            if (rail.change.has_value()) {
                scheduled.push_back(*rail.change);
            }
            // parity: `checkPressed`.
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z, containerSignal);
        }
        // parity: `SculkShriekerBlock.tick`.
        // parity: `SculkCatalystBlock.tick`.
        if (here != 0 &&
            (world::sculkShriekers::isShrieker(here) || world::sculkSpreading::unbloomed(here).has_value()) &&
            options_.foreignTick) {
            options_.foreignTick(world, watchers, x, y, z);
        }
        // parity: `SculkSensorBlock.tick`.
        if (const std::optional<world::BlockChange> sensor =
                world::vibrations::tick(world.state(), x, y, z, world.gameTime());
            sensor.has_value()) {
            // parity: `SculkSensorBlock.updateNeighbours`.
            broadcaster_->announceRedstone(world, watchers, *sensor);
        }
        // parity: `BigDripleafBlock.tick`.
        if (const std::optional<world::BlockChange> leaf =
                world::dripleaves::tick(world.state(), world.name(), x, y, z, world.gameTime());
            leaf.has_value()) {
            scheduled.push_back(*leaf);
        }
        // parity: `LightningRodBlock.tick`.
        if (const std::optional<world::BlockChange> rod = world::lightning::releaseRod(world.state(), x, y, z);
            rod.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *rod);
        }
        // parity: `TargetBlock`.
        if (const std::optional<world::BlockChange> target =
                world::projectileHits::releaseTarget(world.state(), x, y, z);
            target.has_value()) {
            broadcaster_->announceRedstone(world, watchers, *target);
        }
        // parity: `CommandBlock.tick`.
        if (world::commandBlocks::tick(
                world.state(), x, y, z, world.gameTime(), world.gameRules().get(world::gameRules::kCommandBlocksWork),
                world.gameRules().number(world::gameRules::kMaxCommandSequenceLength), runCommand_)) {
            world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                                 world::comparatorReading::of(world.state()));
        }
        // parity: `DispenserBlock.tick`, the four ticks after the signal arrived.
        if (!hooks_.dispenseAllowed || hooks_.dispenseAllowed(world, x, y, z)) {
            const world::fluidFlow::Context context{.world = world.state(),
                                                    .gameRules = world.gameRules(),
                                                    .random = random,
                                                    .ultrawarm =
                                                        world.declaration().dimension == config::WorldDimension::Nether,
                                                    .blockTags = options_.blockTags};
            const world::dispensers::Result result = world::dispensers::fire(
                context, x, y, z,
                {.allocateEntityId = [this] { return players_->allocateEntityId(); },
                 .isCopper =
                     [this](std::int32_t stateId) {
                         const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
                         return block != nullptr && options_.blockTags != nullptr &&
                                world::realBlockTags::contains(*options_.blockTags, "minecraft:copper", *block);
                     },
                 .recipeFor =
                     [this](server::world::crafting::Cooker cooker, const server::world::loot::ItemStack& stack) {
                         return recipes_ == nullptr ? nullptr : recipes_->cookingFor(cooker, stack);
                     },
                 .gameTime = world.gameTime(),
                 .isWitherBase =
                     [this](std::int32_t stateId) {
                         const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
                         return block != nullptr && options_.blockTags != nullptr &&
                                world::realBlockTags::contains(*options_.blockTags,
                                                               "minecraft:wither_summon_base_blocks", *block);
                     },
                 .skullPlaced =
                     [this, &world](std::int32_t sx, std::int32_t sy, std::int32_t sz) {
                         if (options_.skullPlaced) {
                             options_.skullPlaced(world, sx, sy, sz);
                         }
                     },
                 .convertsToMud =
                     [this](const wire::common::Identifier& block) {
                         return options_.blockTags != nullptr &&
                                world::realBlockTags::contains(*options_.blockTags, "minecraft:convertible_to_mud",
                                                               block);
                     },
                 .launchFirework =
                     [this, &world](double fx, double fy, double fz, double dirX, double dirY, double dirZ,
                                    const server::world::loot::ItemStack& stack) {
                         if (options_.launchFirework) {
                             options_.launchFirework(world, fx, fy, fz, dirX, dirY, dirZ, stack);
                         }
                     },
                 .releaseHive =
                     [this, &world](std::int32_t hx, std::int32_t hy, std::int32_t hz, std::int32_t stateId) {
                         if (options_.releaseHive) {
                             options_.releaseHive(world, hx, hy, hz, stateId);
                         }
                     },
                 .brushArmadillo =
                     [this, &world](entity::ServerEntity& armadillo) {
                         return options_.brushArmadillo != nullptr && options_.brushArmadillo(world, armadillo);
                     }});
            if (!result.effects.empty()) {
                broadcaster_->sendEffects(watchers, x, y, z, world.state().blockState(x, y, z).value_or(0),
                                          result.effects);
            }
            // parity: `Level.setBlock`.
            for (const world::BlockChange& change : result.changes) {
                broadcaster_->announceRedstone(world, watchers, change);
                broadcaster_->notifyAround(world, watchers, change.x, change.y, change.z, std::nullopt);
            }
        }
        // parity: `CrafterBlock.tick`, four ticks after the signal arrived.
        if (const std::optional<world::BlockChange> crafted = world::crafters::craft(
                world.state(), x, y, z,
                {.resultFor =
                     [this](const server::world::crafting::CraftingInput& input) { return recipes_->resultFor(input); },
                 .allocateEntityId = [this] { return players_->allocateEntityId(); },
                 .crafted =
                     [this, &world, x, y, z](const server::world::crafting::CraftingInput& input,
                                             const world::blockInventory::Slots& grid) {
                         crafterCrafted(world, x, y, z, input, grid);
                     }});
            crafted.has_value()) {
            scheduled.push_back(*crafted);
        }
        // parity: `RedstoneTorchBlock.tick`.
        if (const std::optional<world::BlockChange> moved = world::redstoneComponents::tick(
                world.state(), world.name(), x, y, z, world.gameTime(), containerSignal);
            moved.has_value()) {
            if (moved->stateId != here) {
                scheduled.push_back(*moved);
            }
            const std::vector<world::BlockChange> settled =
                world::redstone::settle(world.state(), world.name(), moved->x, moved->y, moved->z);
            scheduled.insert(scheduled.end(), settled.begin(), settled.end());
            world::redstoneComponents::scheduleAround(world.state(), world.name(), moved->x, moved->y, moved->z, false,
                                                      containerSignal);
            // parity: `UPDATE_NEIGHBORS`.
            broadcaster_->reshapeAround(world, watchers, moved->x, moved->y, moved->z);
            const std::vector<world::BlockChange> held =
                world::redstoneComponents::relockAround(world.state(), moved->x, moved->y, moved->z);
            scheduled.insert(scheduled.end(), held.begin(), held.end());
            for (const world::BlockChange& change : settled) {
                world::redstoneComponents::scheduleAroundWire(world.state(), world.name(), change.x, change.y, change.z,
                                                              containerSignal);
            }
            world::redstoneComponents::watchFor(world.state(), moved->x, moved->y, moved->z);
            for (const world::BlockChange& change : settled) {
                world::redstoneComponents::watchFor(world.state(), change.x, change.y, change.z);
            }
            // parity: `Level.updateNeighborsAt`.
            const std::vector<world::BlockChange> reacted = world::redstoneReactors::update(
                world.state(), world.name(), moved->x, moved->y, moved->z, world.gameTime(), random,
                world.gameRules().get(world::gameRules::kTntExplodes));
            scheduled.insert(scheduled.end(), reacted.begin(), reacted.end());
            // parity: `UPDATE_ALL`.
            const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
            const world::pistons::Context pistonContext{.world = world.state(),
                                                        .worldName = world.name(),
                                                        .minY = facts.chunkHeight.minY(),
                                                        .maxY = facts.chunkHeight.maxY(),
                                                        .random = random,
                                                        .gameTime = world.gameTime()};
            world::pistons::trigger(pistonContext, moved->x, moved->y, moved->z);
            // parity: `DispenserBlock.neighborChanged`.
            if (const std::optional<world::BlockChange> armed =
                    world::dispensers::onNeighbourChange(world.state(), world.name(), moved->x, moved->y, moved->z);
                armed.has_value()) {
                scheduled.push_back(*armed);
            }
            for (const server::registry::BlockFace face :
                 {server::registry::BlockFace::Down, server::registry::BlockFace::Up,
                  server::registry::BlockFace::North, server::registry::BlockFace::South,
                  server::registry::BlockFace::West, server::registry::BlockFace::East}) {
                world::pistons::trigger(pistonContext, moved->x + server::registry::stepX(face),
                                        moved->y + server::registry::stepY(face),
                                        moved->z + server::registry::stepZ(face));
                if (const std::optional<world::BlockChange> armed = world::dispensers::onNeighbourChange(
                        world.state(), world.name(), moved->x + server::registry::stepX(face),
                        moved->y + server::registry::stepY(face), moved->z + server::registry::stepZ(face));
                    armed.has_value()) {
                    scheduled.push_back(*armed);
                }
            }
            // parity: `UPDATE_ALL`.
            for (const world::redstoneComponents::Reach& at :
                 world::redstoneComponents::outputReach(moved->stateId, moved->x, moved->y, moved->z)) {
                if (!at.skip.has_value()) {
                    continue;
                }
                world::redstoneComponents::scheduleInFront(world.state(), world.name(), at.x, at.y, at.z, *at.skip,
                                                           containerSignal);
                const std::vector<world::BlockChange> ahead = world::redstoneReactors::updateInFront(
                    world.state(), world.name(), at.x, at.y, at.z, *at.skip, world.gameTime(), random,
                    world.gameRules().get(world::gameRules::kTntExplodes));
                scheduled.insert(scheduled.end(), ahead.begin(), ahead.end());
                // parity: `updateNeighborsInFront`.
                {
                    const std::int32_t fx = at.x;
                    const std::int32_t fy = at.y;
                    const std::int32_t fz = at.z;
                    world::pistons::trigger(pistonContext, fx, fy, fz);
                    if (const std::optional<world::BlockChange> armed =
                            world::dispensers::onNeighbourChange(world.state(), world.name(), fx, fy, fz);
                        armed.has_value()) {
                        scheduled.push_back(*armed);
                    }
                    if (const std::optional<world::BlockChange> armed =
                            world::crafters::onNeighbourChange(world.state(), world.name(), fx, fy, fz);
                        armed.has_value()) {
                        scheduled.push_back(*armed);
                    }
                    if (const std::optional<std::int32_t> bent =
                            world::rails::onNeighbourChange(world.state(), world.name(), fx, fy, fz, moved->stateId);
                        bent.has_value()) {
                        scheduled.push_back(world::BlockChange{.x = fx, .y = fy, .z = fz, .stateId = *bent});
                    }
                }
                // parity: `updateNeighborsAtExceptFromFacing`.
                for (const server::registry::BlockFace face :
                     {server::registry::BlockFace::West, server::registry::BlockFace::East,
                      server::registry::BlockFace::Down, server::registry::BlockFace::Up,
                      server::registry::BlockFace::North, server::registry::BlockFace::South}) {
                    if (face == *at.skip) {
                        continue;
                    }
                    const std::int32_t nx = at.x + server::registry::stepX(face);
                    const std::int32_t ny = at.y + server::registry::stepY(face);
                    const std::int32_t nz = at.z + server::registry::stepZ(face);
                    world::pistons::trigger(pistonContext, nx, ny, nz);
                    if (const std::optional<world::BlockChange> armed =
                            world::dispensers::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                        armed.has_value()) {
                        scheduled.push_back(*armed);
                    }
                    // parity: `CrafterBlock.neighborChanged`.
                    if (const std::optional<world::BlockChange> armed =
                            world::crafters::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                        armed.has_value()) {
                        scheduled.push_back(*armed);
                    }
                    // parity: `RailBlock.updateState`.
                    if (const std::optional<std::int32_t> bent =
                            world::rails::onNeighbourChange(world.state(), world.name(), nx, ny, nz, moved->stateId);
                        bent.has_value()) {
                        scheduled.push_back(world::BlockChange{.x = nx, .y = ny, .z = nz, .stateId = *bent});
                    }
                }
            }
            for (const world::BlockChange& change : settled) {
                const std::vector<world::BlockChange> around = world::redstoneReactors::updateAroundWire(
                    world.state(), world.name(), change.x, change.y, change.z, world.gameTime(), random,
                    world.gameRules().get(world::gameRules::kTntExplodes));
                scheduled.insert(scheduled.end(), around.begin(), around.end());
                // parity: `updateNeighborsAt`.
                for (const std::array<std::int32_t, 3>& at :
                     world::redstoneComponents::wireReach(change.x, change.y, change.z)) {
                    for (const server::registry::BlockFace face :
                         {server::registry::BlockFace::West, server::registry::BlockFace::East,
                          server::registry::BlockFace::Down, server::registry::BlockFace::Up,
                          server::registry::BlockFace::North, server::registry::BlockFace::South}) {
                        const std::int32_t nx = at[0] + server::registry::stepX(face);
                        const std::int32_t ny = at[1] + server::registry::stepY(face);
                        const std::int32_t nz = at[2] + server::registry::stepZ(face);
                        world::pistons::trigger(pistonContext, nx, ny, nz);
                        if (const std::optional<world::BlockChange> armed =
                                world::dispensers::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                            armed.has_value()) {
                            scheduled.push_back(*armed);
                        }
                        if (const std::optional<world::BlockChange> armed =
                                world::crafters::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                            armed.has_value()) {
                            scheduled.push_back(*armed);
                        }
                        // parity: the same for the dust, which is a signal source too.
                        if (const std::optional<std::int32_t> bent = world::rails::onNeighbourChange(
                                world.state(), world.name(), nx, ny, nz, change.stateId);
                            bent.has_value()) {
                            scheduled.push_back(world::BlockChange{.x = nx, .y = ny, .z = nz, .stateId = *bent});
                        }
                    }
                }
            }
        }
        world::scheduledBlocks::frogspawn(world.state(), x, y, z, random, scheduled, hatched);
        releaseHatchlings(world, hatched);
        // parity: `SugarCaneBlock.tick`.
        const std::int32_t standing = world.state().blockState(x, y, z).value_or(0);
        const std::size_t before = scheduled.size();
        world::scheduledBlocks::breakIfStillLoose(
            world.state(), x, y, z,
            world::blockSurvival::isSupported(
                world.state(), x, y, z, std::nullopt,
                [&world](const world::WorldState&, std::int32_t sx, std::int32_t sy, std::int32_t sz) {
                    return world::scheduledBlocks::scaffoldingHasFallen(world.state(), sx, sy, sz);
                },
                hasSkylight),
            scheduled);
        if (scheduled.size() != before) {
            broadcaster_->dropBrokenBlock(world, x, y, z, standing);
            // parity: `destroyBlock`.
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        // parity: `FarmlandBlock.turnToDirt`.
        if (const std::optional<std::int32_t> soil = world::scheduledBlocks::coveredSoil(world.state(), x, y, z);
            soil.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *soil});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
            if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
                world::vibrations::emit(world.state(), "block_change", static_cast<double>(x) + 0.5,
                                        static_cast<double>(y) + 0.5, static_cast<double>(z) + 0.5, world.gameTime());
            }
        }
        // parity: `LiquidBlock.tick`.
        world::scheduledBlocks::bubbleColumn(
            world.state(), x, y, z, scheduled,
            [this, &world, &watchers](std::int32_t cx, std::int32_t cy, std::int32_t cz) {
                broadcaster_->reshapeAround(world, watchers, cx, cy, cz);
            });
        // parity: `CoralBlock.tick`.
        if (const std::optional<std::int32_t> dead = world::scheduledBlocks::coral(world.state(), x, y, z);
            dead.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *dead});
            broadcaster_->reshapeAround(world, watchers, x, y, z);
        }
        // parity: `AbstractCauldronBlock.tick`.
        if (const std::optional<world::stalactiteDrips::Fill> fill =
                world::stalactiteDrips::fillCauldron(world.state(), x, y, z, !hasSkylight);
            fill.has_value()) {
            world.state().setBlockState(x, y, z, fill->stateId);
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = fill->stateId});
            broadcaster_->sendEffects(watchers, x, y, z, fill->stateId,
                                      {world::blockEffect::Effect{.type = fill->levelEvent}});
            broadcaster_->notifyAround(world, watchers, x, y, z, here);
        }
        // parity: `CreakingHeartBlock.tick`.
        if (const std::optional<std::int32_t> heart = world::scheduledBlocks::creakingHeartTick(
                world.state(), x, y, z,
                [this](std::int32_t stateId) {
                    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
                    return block != nullptr && options_.blockTags != nullptr &&
                           world::realBlockTags::contains(*options_.blockTags, "minecraft:pale_oak_logs", *block);
                },
                world::scheduledBlocks::creakingIsActive(
                    world.clock().totalTicks(), config::dimensionFacts(world.declaration().dimension).hasDayTimeline));
            heart.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *heart});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        // parity: `SpeleothemBlock.tick`.
        const bool speleothemSupported = world::blockSurvival::isSupported(
            world.state(), x, y, z, std::nullopt,
            [&world](const world::WorldState&, std::int32_t sx, std::int32_t sy, std::int32_t sz) {
                return world::scheduledBlocks::scaffoldingHasFallen(world.state(), sx, sy, sz);
            },
            hasSkylight);
        const world::scheduledBlocks::SpeleothemTurn spike =
            world::scheduledBlocks::speleothemTick(world.state(), x, y, z, speleothemSupported);
        if (spike.destroys) {
            const std::int32_t was = world.state().blockState(x, y, z).value_or(0);
            world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId);
            broadcaster_->dropBrokenBlock(world, x, y, z, was);
            scheduled.push_back(
                world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        } else if (spike.falls) {
            for (std::int32_t at = y; world::scheduledBlocks::isStalactiteAt(world.state(), x, at, z); --at) {
                const bool tip = world::scheduledBlocks::isStalactiteTipAt(world.state(), x, at, z);
                const std::int32_t was = world.state().blockState(x, at, z).value_or(0);
                static_cast<void>(world::fallingBlocks::fallFrom(world.state(), x, at, z,
                                                                 [this] { return players_->allocateEntityId(); }));
                scheduled.push_back(
                    world::BlockChange{.x = x, .y = at, .z = z, .stateId = server::registry::blocks::air().stateId});
                // parity: `FallingBlockEntity.fall`.
                broadcaster_->notifyAround(world, watchers, x, at, z, was);
                if (tip) {
                    break;
                }
            }
        }
        // parity: `ScaffoldingBlock.tick`.
        const world::scheduledBlocks::ScaffoldingTurn scaffold =
            world::scheduledBlocks::scaffoldingTick(world.state(), x, y, z);
        if (scaffold.reshaped.has_value()) {
            scheduled.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *scaffold.reshaped});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        } else if (scaffold.falls || scaffold.breaks) {
            const std::int32_t was = world.state().blockState(x, y, z).value_or(0);
            if (scaffold.falls) {
                static_cast<void>(world::fallingBlocks::fallFrom(world.state(), x, y, z,
                                                                 [this] { return players_->allocateEntityId(); }));
            } else {
                world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId);
                broadcaster_->dropBrokenBlock(world, x, y, z, was);
            }
            scheduled.push_back(
                world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId});
            broadcaster_->notifyAround(world, watchers, x, y, z, std::nullopt);
        }
        world::scheduledBlocks::snifferEgg(world.state(), x, y, z, random, scheduled, hatched);
        releaseHatchlings(world, hatched);
        if (!scheduled.empty()) {
            sendBlockUpdates(watchers, scheduled);
            continue;
        }
        // parity: `ButtonBlock.checkPressed`.
        if (world::arrowButtons::heldByArrow(world.state(), x, y, z, here)) {
            if (const std::optional<std::int32_t> pressed = world::blockInteractions::pressedTicks(here);
                pressed.has_value()) {
                if (const wire::common::Identifier* button = server::registry::blockStateDefinitions::blockOf(here);
                    button != nullptr) {
                    world.state().scheduleBlockTick(x, y, z, *button, *pressed);
                }
            }
            continue;
        }
        const std::optional<world::blockInteractions::BlockRelease> released =
            world::blockInteractions::release(world.state(), x, y, z);
        if (!released.has_value()) {
            continue;
        }
        // parity: the `BLOCK_DEACTIVATE` of `checkPressed`.
        if (world.state().hasSculkSensorNear(x, y, z, world::vibrations::kCalibratedRange)) {
            world::vibrations::emit(world.state(), "block_deactivate", x + 0.5, y + 0.5, z + 0.5, world.gameTime());
        }
        // parity: `ButtonBlock.tick`.
        broadcaster_->announceRedstone(world, watchers,
                                       world::BlockChange{.x = x, .y = y, .z = z, .stateId = released->stateId});
        // parity: `updateNeighbours`.
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(released->stateId, x, y, z);
            wall.has_value()) {
            broadcaster_->announceRedstone(
                world, watchers,
                world::BlockChange{.x = (*wall)[0],
                                   .y = (*wall)[1],
                                   .z = (*wall)[2],
                                   .stateId =
                                       world.state().blockState((*wall)[0], (*wall)[1], (*wall)[2]).value_or(0)});
        }
        sendBlockUpdates(watchers, {world::BlockChange{.x = x, .y = y, .z = z, .stateId = released->stateId}});
        for (PlayerSession* player : watchers) {
            if (player->connection().playSound) {
                player->connection().playSound(released->sound, x + 0.5, y + 0.5, z + 0.5, 1.0F, 1.0F);
                continue;
            }
            if (!player->connection().sendPlayPacket) {
                continue;
            }
            player->connection().sendPlayPacket(SoundPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SoundPacket{.sound = released->sound,
                            .source = SoundSource::Blocks,
                            .x = x + 0.5,
                            .y = y + 0.5,
                            .z = z + 0.5,
                            .volume = 1.0F,
                            .pitch = 1.0F,
                            .seed = options_.nextSeed ? options_.nextSeed() : 0}
                    .write(buffer);
            });
        }
    }
    // parity: `LevelTicks.cleanupAfterTick`.
    world.state().endBlockTicks();
}

} // namespace basalt::server::runtime
