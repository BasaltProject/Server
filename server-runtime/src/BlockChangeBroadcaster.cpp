#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
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
#include <utility>

namespace basalt::server::runtime {
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
                                            .hasSkylight = facts.hasSkylight};
}

} // namespace

BlockChangeBroadcaster::BlockChangeBroadcaster(player::PlayerRegistry& players, world::RealLootTables& lootTables,
                                               Hooks hooks, Options options)
    : players_(&players), lootTables_(&lootTables), hooks_(std::move(hooks)), options_(std::move(options)) {}

std::int64_t BlockChangeBroadcaster::nextSeed() const { return options_.nextSeed ? options_.nextSeed() : 0; }

void BlockChangeBroadcaster::notifyAround(world::LoadedWorld& world,
                                          const std::vector<player::PlayerSession*>& watchers, std::int32_t x,
                                          std::int32_t y, std::int32_t z) {
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const world::neighbourUpdates::Context context = contextOf(world, random);
    std::vector<world::BlockChange> effects;
    world::neighbourUpdates::afterChange(
        context, x, y, z, effects,
        [this, &world, &watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
            dropWashedAwayBlock(world, bx, by, bz, stateId);
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
    random::RandomSource random = random::RandomSource::legacy(nextSeed());
    const std::vector<world::BlockChange> settled =
        world::redstone::settle(world.state(), world.name(), change.x, change.y, change.z);
    for (const world::BlockChange& moved : settled) {
        announceBlock(watchers, moved);
    }
    for (const world::BlockChange& reacted :
         world::redstoneReactors::update(world.state(), world.name(), change.x, change.y, change.z, random)) {
        announceBlock(watchers, reacted);
    }
    // parity: every piece of dust that moved tells its own neighbours, so a lamp at the end of the
    // line lights rather than waiting for something to touch it.
    for (const world::BlockChange& moved : settled) {
        for (const world::BlockChange& reacted :
             world::redstoneReactors::update(world.state(), world.name(), moved.x, moved.y, moved.z, random)) {
            announceBlock(watchers, reacted);
        }
    }
    world::redstoneComponents::scheduleAround(world.state(), change.x, change.y, change.z);
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
    const world::pistons::Context context{.world = world.state(),
                                          .worldName = world.name(),
                                          .minY = facts.chunkHeight.minY(),
                                          .maxY = facts.chunkHeight.maxY(),
                                          .random = random};
    return world::pistons::finish(
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
    const std::optional<std::int32_t> now = world.state().blockState(x, y, z);
    if (!now.has_value() || world::fluidFlow::kindOf(*now) != world::FluidKind::Water) {
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
        static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                                     .uuid = wire::common::Uuid::random(),
                                                                     .item = stack.item,
                                                                     .count = stack.count,
                                                                     .x = x + 0.5,
                                                                     .y = y + 0.5,
                                                                     .z = z + 0.5}));
    }
}

} // namespace basalt::server::runtime
