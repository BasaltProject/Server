#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/server/storage/ProgressRepository.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>
#include <basalt/wire/common/timings/Timings.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::plugins {
class PluginEventBus;
} // namespace basalt::server::plugins

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::command {

class CommandTarget;

[[nodiscard]] wire::protocol::play::SetTimePacket timePacketOf(const world::LoadedWorld& world);

class CommandEnvironment {
public:
    using TagMembers =
        std::function<std::optional<std::set<wire::common::Identifier>>(const wire::common::Identifier& tag)>;

    struct Options {
        std::function<const config::GroupsConfig&()> groups;
        std::function<const config::CommandsConfig&()> commands;
        std::function<server::plugins::PluginEventBus*()> pluginEvents;

        random::RandomSource* random = nullptr;

        std::int64_t worldSeed = 0;
        std::string defaultWorldName = "world";
        std::int32_t maxPlayers = 20;
        wire::common::Logger logger{"Command"};
        std::function<void()> stopServer;
        std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           std::int32_t destroyedState)>
            dropBrokenBlock;
        /// parity: `CarvedPumpkinBlock.trySpawnGolem`'s `addFreshEntity`.
        std::function<void(world::LoadedWorld& world, std::string_view type, double x, double y, double z)> summonGolem;
        /// parity: `EnderDragon.kill`.
        std::function<bool(world::LoadedWorld& world, entity::ServerEntity& entity)> killOutright;
        wire::text::LangTable lang;
        storage::PlayerRepository* playerRepository = nullptr;
        storage::ProgressRepository* progressRepository = nullptr;
        storage::AccessRepository* access = nullptr;
        std::map<std::string, world::LoadedWorld*, std::less<>> worlds;
        const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;
        wire::common::Timings* timings = nullptr;

        TagMembers blockTagMembers;
        TagMembers itemTagMembers;
        TagMembers entityTagMembers;

        scoreboard::Scoreboard* scoreboard = nullptr;
        scoreboard::Teams* teams = nullptr;
        const world::RealTimelines* timelines = nullptr;
        const world::RealAdvancements* advancements = nullptr;
        world::PlayerProgress* progress = nullptr;
        world::AdvancementTriggers* advancementTriggers = nullptr;
        /// parity: `predicate=` and `execute if predicate`.
        std::function<std::optional<bool>(const wire::common::JsonValue& predicate, const CommandTarget* self,
                                          world::LoadedWorld* world, const std::array<double, 3>& origin)>
            predicate;
    };

    CommandEnvironment(player::PlayerRegistry& players, Options options);

    [[nodiscard]] player::PlayerRegistry& players() const noexcept { return *players_; }

    [[nodiscard]] const Options& options() const noexcept { return options_; }

    void setWorlds(std::map<std::string, world::LoadedWorld*, std::less<>> worlds) {
        options_.worlds = std::move(worlds);
    }

    [[nodiscard]] const wire::common::Logger& logger() const noexcept { return options_.logger; }

    [[nodiscard]] const wire::text::LangTable& lang() const noexcept { return options_.lang; }

    [[nodiscard]] const config::GroupsConfig& groups() const;

    [[nodiscard]] const config::CommandsConfig& commands() const;

    [[nodiscard]] server::plugins::PluginEventBus* pluginEvents() const;

    [[nodiscard]] const wire::common::placeholder::PlaceholderRegistry& placeholders() const;

    void broadcast(const wire::text::Component& message) const;

    [[nodiscard]] std::vector<player::PlayerSession*> playersIn(const world::LoadedWorld& world) const;

    void blockEffects(const world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t stateId, const std::vector<world::BlockEffect>& effects) const;

    void syncTime(const world::LoadedWorld& world) const;

    void syncChunks(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                    std::int32_t maxZ) const;

    void syncBiomes(const world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                    std::int32_t maxZ) const;

    void syncWeather(const world::LoadedWorld& world) const;

private:
    player::PlayerRegistry* players_;
    Options options_;
    config::GroupsConfig emptyGroups_;
    config::CommandsConfig emptyCommands_;
    wire::common::placeholder::PlaceholderRegistry ownPlaceholders_;
};

} // namespace basalt::server::runtime::command
