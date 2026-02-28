#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/wire/text/TranslationResolver.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::command {

wire::protocol::play::SetTimePacket timePacketOf(const world::LoadedWorld& world) {
    const std::int32_t clockId = world.declaration().dimension == config::WorldDimension::End
                                     ? wire::protocol::play::SetTimePacket::kEndClock
                                     : wire::protocol::play::SetTimePacket::kOverworldClock;
    return wire::protocol::play::SetTimePacket{
        .gameTime = world.gameTime(),
        .clocks = {wire::protocol::play::ClockUpdate{
            .clockId = clockId,
            .totalTicks = world.clock().totalTicks(),
            .partialTick = 0.0F,
            .rate = world.clock().networkRate(world.gameRules().get(world::gameRules::kAdvanceTime))}}};
}

CommandEnvironment::CommandEnvironment(player::PlayerRegistry& players, Options options)
    : players_(&players), options_(std::move(options)), emptyGroups_({}, 0, 0), emptyCommands_({}, {}, {}) {}

const config::GroupsConfig& CommandEnvironment::groups() const {
    return options_.groups ? options_.groups() : emptyGroups_;
}

const config::CommandsConfig& CommandEnvironment::commands() const {
    return options_.commands ? options_.commands() : emptyCommands_;
}

server::plugins::PluginEventBus* CommandEnvironment::pluginEvents() const {
    return options_.pluginEvents ? options_.pluginEvents() : nullptr;
}

const wire::common::placeholder::PlaceholderRegistry& CommandEnvironment::placeholders() const {
    return options_.placeholders == nullptr ? ownPlaceholders_ : *options_.placeholders;
}

void CommandEnvironment::broadcast(const wire::text::Component& message) const {
    for (player::PlayerSession* player : players_->snapshot()) {
        player->sendSystemMessage(message);
    }
    options_.logger.info([&] { return wire::text::resolvePlainText(message, options_.lang); });
}

std::vector<player::PlayerSession*> CommandEnvironment::playersIn(const world::LoadedWorld& world) const {
    std::vector<player::PlayerSession*> inWorld;
    for (player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            inWorld.push_back(player);
        }
    }
    return inWorld;
}

void CommandEnvironment::blockEffects(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                      std::int32_t stateId, const std::vector<world::BlockEffect>& effects) const {
    if (effects.empty()) {
        return;
    }
    // One is enough: a connection sends these to everyone in its own world, not only to itself.
    const std::vector<player::PlayerSession*> inWorld = playersIn(world);
    if (inWorld.empty() || !inWorld.front()->connection().broadcastEffects) {
        return;
    }
    inWorld.front()->connection().broadcastEffects(x, y, z, stateId, effects);
}

void CommandEnvironment::syncTime(const world::LoadedWorld& world) const {
    const wire::protocol::play::SetTimePacket packet = timePacketOf(world);
    for (player::PlayerSession* player : playersIn(world)) {
        // A client that speaks the other edition is told the same thing its own way: left out of
        // this, its sun stops where it stood when they arrived and never moves again.
        if (player->connection().sendTime) {
            player->connection().sendTime(world);
            continue;
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetTimePacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void CommandEnvironment::syncChunks(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ,
                                    std::int32_t maxX, std::int32_t maxZ) const {
    const std::vector<player::PlayerSession*> inWorld = playersIn(world);
    if (inWorld.empty()) {
        return;
    }
    for (std::int32_t chunkX = minX >> 4; chunkX <= (maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = minZ >> 4; chunkZ <= (maxZ >> 4); chunkZ++) {
            for (player::PlayerSession* player : inWorld) {
                if (player->connection().resendChunk) {
                    player->connection().resendChunk(chunkX, chunkZ);
                }
            }
        }
    }
}

void CommandEnvironment::syncBiomes(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ,
                                    std::int32_t maxX, std::int32_t maxZ) const {
    const std::vector<player::PlayerSession*> inWorld = playersIn(world);
    if (inWorld.empty()) {
        return;
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> columns;
    for (std::int32_t chunkX = minX >> 4; chunkX <= (maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = minZ >> 4; chunkZ <= (maxZ >> 4); chunkZ++) {
            columns.emplace_back(chunkX, chunkZ);
        }
    }
    for (player::PlayerSession* player : inWorld) {
        if (player->connection().sendBiomes) {
            player->connection().sendBiomes(columns);
        }
    }
}

void CommandEnvironment::syncWeather(const world::LoadedWorld& world) const {
    for (player::PlayerSession* player : playersIn(world)) {
        if (player->connection().sendWeather) {
            player->connection().sendWeather(world.weather());
        }
    }
}

} // namespace basalt::server::runtime::command
