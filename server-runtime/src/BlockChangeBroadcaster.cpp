#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/GolemPatterns.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// parity: `EntityTypes.ITEM.getHeight() / 2`.
constexpr double kItemHalfHeight = 0.125;

/// parity: the y of `ItemEntity`'s own `setDeltaMovement`.
constexpr double kPoppedUpwards = 0.2;

} // namespace

namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;

void sendBlock(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId) {
    for (const player::PlayerSession* player : watchers) {
        player::sendBlockChangeTo(*player, x, y, z, stateId);
    }
}

[[nodiscard]] world::neighbourUpdates::Context contextOf(world::LoadedWorld& world, random::RandomSource& random,
                                                         world::neighbourUpdates::Context::SummonGolem summonGolem,
                                                         world::golemPatterns::IsCopper isCopper) {
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
        .wakeFluids =
            [&world, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                const world::fluidFlow::Context flow{.world = world.state(),
                                                     .gameRules = world.gameRules(),
                                                     .random = random,
                                                     .ultrawarm = world.declaration().dimension ==
                                                                  config::WorldDimension::Nether};
                static_cast<void>(world::fluidFlow::onBlockChanged(flow, x, y, z));
            },
        .arrivedFluid =
            [&world, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                const world::fluidFlow::Context flow{.world = world.state(),
                                                     .gameRules = world.gameRules(),
                                                     .random = random,
                                                     .ultrawarm = world.declaration().dimension ==
                                                                  config::WorldDimension::Nether};
                world::fluidFlow::onArrived(flow, x, y, z);
            },
        // parity: `PotentSulfurBlock.updateShape`.
        .geyser =
            [&world](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                return world::geysers::validState(world.state(), world.gameTime(), x, y, z, stateId);
            },
        .summonGolem = std::move(summonGolem),
        .isCopper = std::move(isCopper),
        .gameTime = world.gameTime(),
        .tntExplodes = world.gameRules().get(world::gameRules::kTntExplodes)};
}

} // namespace

BlockChangeBroadcaster::BlockChangeBroadcaster(player::PlayerRegistry& players, world::RealLootTables& lootTables,
                                               Hooks hooks, Options options)
    : players_(&players), lootTables_(&lootTables), hooks_(std::move(hooks)), options_(std::move(options)) {}

std::int64_t BlockChangeBroadcaster::nextSeed() const { return options_.nextSeed ? options_.nextSeed() : 0; }

world::neighbourUpdates::Context BlockChangeBroadcaster::updateContext(world::LoadedWorld& world,
                                                                       random::RandomSource& random) const {
    return contextOf(
        world, random,
        [this, &world](world::golemPatterns::Golem kind, double gx, double gy, double gz) {
            if (hooks_.summonGolem) {
                hooks_.summonGolem(world, world::golemPatterns::entityTypeOf(kind), gx, gy, gz);
            }
        },
        [this](std::int32_t stateId) {
            const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
            return block != nullptr && options_.blockTags != nullptr &&
                   world::realBlockTags::contains(*options_.blockTags, "minecraft:copper", *block);
        });
}

void BlockChangeBroadcaster::notifyAround(world::LoadedWorld& world,
                                          const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                          std::int32_t y, std::int32_t z, std::optional<std::int32_t> removed) {
    settleBlock(world, watchers, x, y, z, removed, world::neighbourUpdates::phase::kAll);
}

void BlockChangeBroadcaster::settleBlock(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                                         std::int32_t x, std::int32_t y, std::int32_t z,
                                         std::optional<std::int32_t> removed, std::uint32_t phases) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    world::neighbourUpdates::Context context = updateContext(world, random);
    context.phases = phases;
    std::vector<world::BlockChange> effects;
    world::neighbourUpdates::afterChange(
        context, x, y, z, removed, effects,
        [this, &world, &watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
            dropBrokenBlock(world, bx, by, bz, stateId);
        },
        [&watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, stateId);
        },
        world::neighbourUpdates::ArrivedShape::NotPlaced);
    for (const world::BlockChange& change : effects) {
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects);
    }
}

void BlockChangeBroadcaster::neighbourChanged(world::LoadedWorld& world,
                                              const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::optional<std::int32_t> source) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::neighbourUpdates::Context context = updateContext(world, random);
    std::vector<world::BlockChange> effects;
    world::neighbourUpdates::neighbourChanged(
        context, x, y, z, source, effects,
        [this, &world, &watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
            dropBrokenBlock(world, bx, by, bz, stateId);
        },
        [&watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, stateId);
        });
    for (const world::BlockChange& change : effects) {
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects);
    }
}

void BlockChangeBroadcaster::reshapeIndirect(world::LoadedWorld& world,
                                             const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                             std::int32_t y, std::int32_t z, std::int32_t stateId) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::neighbourUpdates::Context context = updateContext(world, random);
    world::neighbourUpdates::reshapeIndirect(context, x, y, z, stateId,
                                             [&watchers](std::int32_t shaped, std::int32_t bx, std::int32_t by,
                                                         std::int32_t bz) { sendBlock(watchers, bx, by, bz, shaped); });
}

void BlockChangeBroadcaster::reshapeAround(world::LoadedWorld& world,
                                           const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                           std::int32_t y, std::int32_t z) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::neighbourUpdates::Context context = updateContext(world, random);
    world::neighbourUpdates::reshapeOnly(
        context, x, y, z,
        [this, &world, &watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
            dropBrokenBlock(world, bx, by, bz, stateId);
        },
        [&watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, stateId);
        });
}

void BlockChangeBroadcaster::announceBlock(const std::vector<player::PlayerSession*>& watchers,
                                           const world::BlockChange& change) const {
    sendBlock(watchers, change.x, change.y, change.z, change.stateId);
    if (!change.effects.empty()) {
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects);
    }
}

void BlockChangeBroadcaster::sendEffects(const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                         std::int32_t y, std::int32_t z, std::int32_t stateId,
                                         const std::vector<world::BlockEffect>& effects) const {
    for (const world::BlockEffect& effect : effects) {
        if (const auto* sound = std::get_if<world::blockEffect::Sound>(&effect); sound != nullptr) {
            const std::int64_t seed = nextSeed();
            for (const player::PlayerSession* player : watchers) {
                player::playSoundFor(*player, sound->name, sound->source, x + 0.5, y + 0.5, z + 0.5, sound->volume,
                                     sound->pitch, seed);
            }
        } else if (const auto* event = std::get_if<world::blockEffect::Event>(&effect); event != nullptr) {
            for (const player::PlayerSession* player : watchers) {
                if (player->connection().sendBlockEvent) {
                    player->connection().sendBlockEvent(x, y, z, stateId, event->b0, event->b1);
                }
            }
        } else if (const auto* level = std::get_if<world::blockEffect::Effect>(&effect); level != nullptr) {
            for (const player::PlayerSession* player : watchers) {
                if (player->connection().sendLevelEvent) {
                    player->connection().sendLevelEvent(level->type, x, y, z, level->data);
                }
            }
        }
    }
}

void BlockChangeBroadcaster::announceRedstone(world::LoadedWorld& world,
                                              const std::vector<player::PlayerSession*>& watchers,
                                              const world::BlockChange& change) const {
    announceBlock(watchers, change);
    settleFrom(world, watchers, change.x, change.y, change.z, true, world::redstone::Asked::WithSelf);
    // parity: `updateNeighborsAt`.
    for (const registry::BlockFace behind : world::redstone::alsoTells(change.stateId)) {
        settleFrom(world, watchers, change.x + registry::stepX(behind), change.y + registry::stepY(behind),
                   change.z + registry::stepZ(behind), false, world::redstone::Asked::NeighboursOnly);
    }
}

void BlockChangeBroadcaster::settleFrom(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                                        const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const bool arrived, const world::redstone::Asked asked) const {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::vector<world::BlockChange> settled =
        world::redstone::settle(world.state(), world.name(), x, y, z, {}, asked);
    for (const world::BlockChange& moved : settled) {
        announceBlock(watchers, moved);
    }
    for (const world::BlockChange& reacted :
         world::redstoneReactors::update(world.state(), world.name(), x, y, z, world.gameTime(), random,
                                         world.gameRules().get(world::gameRules::kTntExplodes))) {
        announceBlock(watchers, reacted);
    }
    for (const world::BlockChange& moved : settled) {
        for (const world::BlockChange& reacted : world::redstoneReactors::updateAroundWire(
                 world.state(), world.name(), moved.x, moved.y, moved.z, world.gameTime(), random,
                 world.gameRules().get(world::gameRules::kTntExplodes))) {
            announceBlock(watchers, reacted);
        }
    }
    const world::redstoneComponents::ContainerSignalAt reading = world::comparatorReading::of(world.state());
    world::redstoneComponents::scheduleAround(world.state(), world.name(), x, y, z, arrived, reading);
    // parity: every piece of dust that moved reaches two squares out, so the component past it hears.
    for (const world::BlockChange& moved : settled) {
        world::redstoneComponents::scheduleAroundWire(world.state(), world.name(), moved.x, moved.y, moved.z, reading);
    }
    // parity: `PistonBaseBlock.checkIfExtend`.
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::pistons::Context piston{.world = world.state(),
                                         .worldName = world.name(),
                                         .minY = facts.chunkHeight.minY(),
                                         .maxY = facts.chunkHeight.maxY(),
                                         .random = random,
                                         .gameTime = world.gameTime()};
    const auto tellAround = [&](std::int32_t cx, std::int32_t cy, std::int32_t cz) {
        const std::optional<std::int32_t> source = world.state().blockState(cx, cy, cz);
        for (const registry::BlockFace face :
             {registry::BlockFace::West, registry::BlockFace::East, registry::BlockFace::Down, registry::BlockFace::Up,
              registry::BlockFace::North, registry::BlockFace::South}) {
            const std::int32_t nx = cx + registry::stepX(face);
            const std::int32_t ny = cy + registry::stepY(face);
            const std::int32_t nz = cz + registry::stepZ(face);
            world::pistons::trigger(piston, nx, ny, nz);
            if (const std::optional<world::BlockChange> armed =
                    world::dispensers::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                armed.has_value()) {
                announceBlock(watchers, *armed);
            }
            // parity: `CrafterBlock.neighborChanged`, the same rising edge with its own four ticks.
            if (const std::optional<world::BlockChange> armed =
                    world::crafters::onNeighbourChange(world.state(), world.name(), nx, ny, nz);
                armed.has_value()) {
                announceBlock(watchers, *armed);
            }
            // parity: `RailBlock.updateState`.
            if (source.has_value()) {
                if (const std::optional<std::int32_t> bent =
                        world::rails::onNeighbourChange(world.state(), world.name(), nx, ny, nz, *source);
                    bent.has_value()) {
                    announceBlock(watchers, world::BlockChange{.x = nx, .y = ny, .z = nz, .stateId = *bent});
                }
            }
        }
    };
    tellAround(x, y, z);
    for (const world::BlockChange& moved : settled) {
        for (const std::array<std::int32_t, 3>& at : world::redstoneComponents::wireReach(moved.x, moved.y, moved.z)) {
            tellAround(at[0], at[1], at[2]);
        }
    }
}

namespace {

/// parity: `Direction`, in the order a block event numbers it.
[[nodiscard]] std::string_view directionName(const std::int32_t index) {
    static constexpr std::array<std::string_view, 6> kNames{"down", "up", "north", "south", "west", "east"};
    return index >= 0 && index < 6 ? kNames[static_cast<std::size_t>(index)] : std::string_view{};
}

} // namespace

void BlockChangeBroadcaster::runPistonEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                                            const std::vector<player::PlayerSession*>& watchers) {
    if (hooks_.pistonAllowed &&
        !hooks_.pistonAllowed(world, event.x, event.y, event.z, event.first == 0, directionName(event.second))) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::pistons::Context context{
        .world = world.state(),
        .worldName = world.name(),
        .minY = facts.chunkHeight.minY(),
        .maxY = facts.chunkHeight.maxY(),
        .random = random,
        .gameTime = world.gameTime(),
        .onDestroyed = [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by,
                                      std::int32_t bz) { dropBrokenBlock(world, bx, by, bz, stateId); },
        .onLanded = [this, &world,
                     &watchers](std::int32_t bx, std::int32_t by,
                                std::int32_t bz) { notifyAround(world, watchers, bx, by, bz, std::nullopt); },
        .withinBorder =
            [&world](std::int32_t bx, std::int32_t bz) {
                return world.border().contains(static_cast<double>(bx), static_cast<double>(bz));
            }};
    std::vector<world::BlockChange> effects;
    world::pistons::runEvent(context, event, effects);
    for (const world::BlockChange& effect : effects) {
        sendEffects(watchers, effect.x, effect.y, effect.z, effect.stateId, effect.effects);
    }
}

std::vector<world::BlockChange>
BlockChangeBroadcaster::finishPiston(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position,
                                     const std::vector<player::PlayerSession*>& watchers) {
    if (hooks_.pistonAllowed && !hooks_.pistonAllowed(world, position[0], position[1], position[2], false, {})) {
        return {};
    }
    std::vector<world::pistons::PlayerPosition> nearby;
    for (const player::PlayerSession* player : watchers) {
        if (std::abs(player->x - position[0]) <= options_.pistonReach &&
            std::abs(player->z - position[2]) <= options_.pistonReach) {
            nearby.push_back({.uuid = player->uuid().toString(), .x = player->x, .y = player->y, .z = player->z});
        }
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::pistons::Context context{
        .world = world.state(),
        .worldName = world.name(),
        .minY = facts.chunkHeight.minY(),
        .maxY = facts.chunkHeight.maxY(),
        .random = random,
        .gameTime = world.gameTime(),
        .onDestroyed = [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by,
                                      std::int32_t bz) { dropBrokenBlock(world, bx, by, bz, stateId); },
        .onLanded = [this, &world,
                     &watchers](std::int32_t bx, std::int32_t by,
                                std::int32_t bz) { notifyAround(world, watchers, bx, by, bz, std::nullopt); },
        .withinBorder =
            [&world](std::int32_t bx, std::int32_t bz) {
                return world.border().contains(static_cast<double>(bx), static_cast<double>(bz));
            }};
    return world::pistons::slideTick(
        context, position[0], position[1], position[2], nearby,
        [&watchers](const world::pistons::PlayerPosition& pushed) {
            for (player::PlayerSession* player : watchers) {
                if (player->uuid().toString() == pushed.uuid && player->connection().teleportTo) {
                    player->connection().teleportTo(pushed.x, pushed.y, pushed.z, player->yaw, player->pitch);
                    return;
                }
            }
        });
}

void BlockChangeBroadcaster::dropFallenBlock(world::LoadedWorld& world, double x, double y, double z,
                                             std::int32_t stateId) {
    if (!world.gameRules().get(world::gameRules::kEntityDrops)) {
        return;
    }
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || reg::items::registry().byName(*block) == nullptr) {
        return;
    }
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    // parity: `ItemEntity`.
    static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                                 .uuid = wire::common::Uuid::random(),
                                                                 .item = *block,
                                                                 .count = 1,
                                                                 .x = x,
                                                                 .y = y,
                                                                 .z = z,
                                                                 .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                                 .velocityY = kPoppedUpwards,
                                                                 .velocityZ = (random.nextDouble() * 0.2) - 0.1}));
}

void BlockChangeBroadcaster::dropWashedAwayBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                 std::int32_t z, std::int32_t destroyedState) {
    if (destroyedState <= 0 || world::fluidFlow::kindOf(destroyedState).has_value()) {
        return;
    }
    // parity: `LavaFluid.beforeDestroyingBlock`.
    const std::optional<std::int32_t> now = world.state().blockState(x, y, z);
    if (!now.has_value() || world::fluidFlow::kindOf(*now) != world::FluidKind::Water) {
        return;
    }
    dropBrokenBlock(world, x, y, z, destroyedState);
}

void BlockChangeBroadcaster::dropBrokenBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t destroyedState, bool byEntity) {
    // parity: `Block.popResource`.
    if (destroyedState <= 0 || !world.gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    const std::optional<reg::BlockState> state = reg::blocks::byStateId(destroyedState);
    if (!state.has_value()) {
        return;
    }
    const server::world::loot::LootTable* table = lootTables_->forBlock(state->block);
    if (table == nullptr) {
        return;
    }
    random::RandomSource scratch = random::RandomSource::legacy(nextSeed());
    random::RandomSource& random =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
    // parity: `LootContextParams.BLOCK_STATE`.
    server::world::loot::LootContext context{.blockState = reg::blockStateDefinitions::propertiesOf(destroyedState),
                                             .block = state->block,
                                             .hasEntity = byEntity};
    if (world::decoratedPots::isPot(state->block.path())) {
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* pot =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (pot != nullptr) {
            context.dynamic.emplace(std::string{world::decoratedPots::kSherds},
                                    world::decoratedPots::sherds(pot->data()));
        }
    }
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, std::nullopt, context)) {
        if (reg::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        // parity: `Block.popResource`.
        const auto scatter = [&random] { return (random.nextDouble() * 0.5) - 0.25; };
        static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                                     .uuid = wire::common::Uuid::random(),
                                                                     .item = stack.item,
                                                                     .count = stack.count,
                                                                     .x = x + 0.5 + scatter(),
                                                                     .y = y + 0.5 + scatter() - kItemHalfHeight,
                                                                     .z = z + 0.5 + scatter(),
                                                                     .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                                     .velocityY = kPoppedUpwards,
                                                                     .velocityZ = (random.nextDouble() * 0.2) - 0.1}));
    }
}

} // namespace basalt::server::runtime
