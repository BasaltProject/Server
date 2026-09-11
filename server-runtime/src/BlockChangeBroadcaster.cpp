#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/wire/protocol/play/BlockEntityPackets.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime {
namespace {

/// parity: `EntityTypes.ITEM.getHeight() / 2`, which `popResource` sits the stack down by.
constexpr double kItemHalfHeight = 0.125;

/// parity: the y of `ItemEntity`'s own `setDeltaMovement`, the little hop a dropped block makes.
constexpr double kPoppedUpwards = 0.2;

} // namespace

namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;

void sendTo(const std::vector<player::PlayerSession*>& watchers, std::int32_t packetId,
            const std::function<void(wire::io::ByteBuffer&)>& write) {
    for (const player::PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(packetId, write);
        }
    }
}

void sendBlock(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId) {
    const play::ClientboundBlockUpdatePacket packet{.pos = play::BlockPos{x, y, z}, .blockStateId = stateId};
    sendTo(watchers, play::ClientboundBlockUpdatePacket::kId,
           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

/// The world's own neighbour-update context, which is what the cascade reads off a world.
[[nodiscard]] world::neighbourUpdates::Context contextOf(world::LoadedWorld& world, random::RandomSource& random) {
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    return world::neighbourUpdates::Context{.world = world.state(),
                                            .worldName = world.name(),
                                            .minY = facts.chunkHeight.minY(),
                                            .maxY = facts.chunkHeight.maxY(),
                                            .random = random,
                                            .hasSkylight = facts.hasSkylight,
                                            .wakeFluids =
                                                [&world, &random](std::int32_t x, std::int32_t y, std::int32_t z) {
                                                    const world::fluidFlow::Context flow{
                                                        .world = world.state(),
                                                        .gameRules = world.gameRules(),
                                                        .random = random,
                                                        .ultrawarm = world.declaration().dimension ==
                                                                     config::WorldDimension::Nether};
                                                    static_cast<void>(world::fluidFlow::onBlockChanged(flow, x, y, z));
                                                },
                                            .gameTime = world.gameTime(),
                                            .tntExplodes = world.gameRules().get(world::gameRules::kTntExplodes)};
}

} // namespace

BlockChangeBroadcaster::BlockChangeBroadcaster(player::PlayerRegistry& players, world::RealLootTables& lootTables,
                                               Hooks hooks, Options options)
    : players_(&players), lootTables_(&lootTables), hooks_(std::move(hooks)), options_(std::move(options)) {}

std::int64_t BlockChangeBroadcaster::nextSeed() const { return options_.nextSeed ? options_.nextSeed() : 0; }

void BlockChangeBroadcaster::notifyAround(world::LoadedWorld& world,
                                          const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                          std::int32_t y, std::int32_t z, std::optional<std::int32_t> removed) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::neighbourUpdates::Context context = contextOf(world, random);
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
        false);
    for (const world::BlockChange& change : effects) {
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects);
    }
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
    const play::BlockPos pos{x, y, z};
    for (const world::BlockEffect& effect : effects) {
        if (const auto* sound = std::get_if<world::blockEffect::Sound>(&effect); sound != nullptr) {
            const play::SoundPacket packet{.sound = sound->name,
                                           .source = sound->source,
                                           .x = x + 0.5,
                                           .y = y + 0.5,
                                           .z = z + 0.5,
                                           .volume = sound->volume,
                                           .pitch = sound->pitch,
                                           .seed = nextSeed()};
            sendTo(watchers, play::SoundPacket::kId, [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        } else if (const auto* event = std::get_if<world::blockEffect::Event>(&effect); event != nullptr) {
            const play::BlockEventPacket packet{.pos = pos,
                                                .b0 = static_cast<std::uint8_t>(event->b0),
                                                .b1 = static_cast<std::uint8_t>(event->b1),
                                                .blockId = reg::blockSupport::blockRegistryIdOf(stateId)};
            sendTo(watchers, play::BlockEventPacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        } else if (const auto* level = std::get_if<world::blockEffect::Effect>(&effect); level != nullptr) {
            const play::LevelEventPacket packet{.event = level->type, .pos = pos, .data = level->data};
            sendTo(watchers, play::LevelEventPacket::kId,
                   [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void BlockChangeBroadcaster::announceRedstone(world::LoadedWorld& world,
                                              const std::vector<player::PlayerSession*>& watchers,
                                              const world::BlockChange& change) const {
    announceBlock(watchers, change);
    settleFrom(world, watchers, change.x, change.y, change.z, true);
    // parity: a rod and a hook reach one square further than their own write does, and what is on
    // the far side of the wall they hang on only hears them through that second update.
    const std::optional<registry::BlockFace> behind = world::redstone::alsoTells(change.stateId);
    if (behind.has_value()) {
        settleFrom(world, watchers, change.x + registry::stepX(*behind), change.y + registry::stepY(*behind),
                   change.z + registry::stepZ(*behind), false);
    }
}

void BlockChangeBroadcaster::settleFrom(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                                        const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                        const bool arrived) const {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::vector<world::BlockChange> settled = world::redstone::settle(world.state(), world.name(), x, y, z);
    for (const world::BlockChange& moved : settled) {
        announceBlock(watchers, moved);
    }
    for (const world::BlockChange& reacted :
         world::redstoneReactors::update(world.state(), world.name(), x, y, z, world.gameTime(), random,
                                         world.gameRules().get(world::gameRules::kTntExplodes))) {
        announceBlock(watchers, reacted);
    }
    // parity: every piece of dust that moved tells its own neighbours, so a lamp at the end of the
    // line lights rather than waiting for something to touch it.
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
    // parity: `PistonBaseBlock.checkIfExtend` and `DispenserBlock.neighborChanged` are the two that
    // nobody else asks, and they hear the same `updateNeighborsAt` as the rest: the six squares
    // around what changed, and the six around each square a dust that moved reaches.
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::pistons::Context piston{.world = world.state(),
                                         .worldName = world.name(),
                                         .minY = facts.chunkHeight.minY(),
                                         .maxY = facts.chunkHeight.maxY(),
                                         .random = random,
                                         .gameTime = world.gameTime()};
    const auto tellAround = [&](std::int32_t cx, std::int32_t cy, std::int32_t cz) {
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
        }
    };
    tellAround(x, y, z);
    for (const world::BlockChange& moved : settled) {
        for (const std::array<std::int32_t, 3>& at : world::redstoneComponents::wireReach(moved.x, moved.y, moved.z)) {
            tellAround(at[0], at[1], at[2]);
        }
    }
}

void BlockChangeBroadcaster::runPistonEvent(world::LoadedWorld& world, const world::BlockEvent& event,
                                            const std::vector<player::PlayerSession*>& watchers) {
    if (hooks_.pistonAllowed && !hooks_.pistonAllowed(world, event.x, event.y, event.z)) {
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
        .onDestroyed = [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            dropBrokenBlock(world, bx, by, bz, stateId);
        }};
    std::vector<world::BlockChange> effects;
    world::pistons::runEvent(context, event.x, event.y, event.z, event.first, event.second, effects);
    for (const world::BlockChange& effect : effects) {
        sendEffects(watchers, effect.x, effect.y, effect.z, effect.stateId, effect.effects);
    }
}

std::vector<world::BlockChange>
BlockChangeBroadcaster::finishPiston(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position,
                                     const std::vector<player::PlayerSession*>& watchers) {
    if (hooks_.pistonAllowed && !hooks_.pistonAllowed(world, position[0], position[1], position[2])) {
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
        .onDestroyed = [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            dropBrokenBlock(world, bx, by, bz, stateId);
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

void BlockChangeBroadcaster::dropWashedAwayBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                 std::int32_t z, std::int32_t destroyedState) {
    if (destroyedState <= 0 || world::fluidFlow::kindOf(destroyedState).has_value()) {
        return;
    }
    // parity: `LavaFluid.beforeDestroyingBlock` only plays the fizz, so only water drops what it
    // washed away.
    const std::optional<std::int32_t> now = world.state().blockState(x, y, z);
    if (!now.has_value() || world::fluidFlow::kindOf(*now) != world::FluidKind::Water) {
        return;
    }
    dropBrokenBlock(world, x, y, z, destroyedState);
}

void BlockChangeBroadcaster::dropBrokenBlock(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t destroyedState) {
    // parity: `Block.popResource` puts nothing in the world with the `block_drops` rule off, so the
    // block still goes and leaves nothing behind.
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
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    for (const server::world::loot::ItemStack& stack : table->generate(random)) {
        if (reg::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        // parity: `Block.popResource` scatters it a quarter of a block either way and sits it half
        // its own height lower, and `ItemEntity`'s own constructor throws it up at 0.2. Dropping it
        // dead still at the middle of the square left it a block lower than the jar's a few ticks
        // later, which is what the bench sees.
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
