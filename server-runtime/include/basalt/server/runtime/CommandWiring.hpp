#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/command/commands/DataPackCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/runtime/command/commands/SaveCommands.hpp>
#include <basalt/server/runtime/command/commands/StopwatchCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace basalt::server::runtime {

class TickLoop;
class VanillaGameData;

namespace config {
class LiveConfigs;
}

namespace function {
class FunctionLibrary;
}

namespace timings {
class TimingsReport;
}

struct CommandWiring {
    player::PlayerRegistry* players = nullptr;
    config::LiveConfigs* live = nullptr;
    storage::PlayerStore* playerStore = nullptr;
    VanillaGameData* gameData = nullptr;
    scoreboard::Scoreboard* scoreboard = nullptr;
    scoreboard::Teams* teams = nullptr;
    world::PlayerProgress* progress = nullptr;
    world::AdvancementTriggers* advancementTriggers = nullptr;
    function::FunctionLibrary* functions = nullptr;
    command::commands::FunctionRunner* functionRunner = nullptr;
    command::commands::FunctionSchedule* functionSchedule = nullptr;
    command::commands::DataPackControl datapacks;
    command::commands::BossBarRegistry* bossBars = nullptr;
    command::CommandStorage* commandStorage = nullptr;
    command::commands::Stopwatches* stopwatches = nullptr;
    command::commands::SaveControl* saveControl = nullptr;
    timings::TimingsReport* timingsReport = nullptr;
    wire::common::Timings* timings = nullptr;
    wire::common::TickDurations* tickDurations = nullptr;
    TickLoop* tickLoop = nullptr;
    const wire::common::placeholder::PlaceholderRegistry* placeholders = nullptr;
    const wire::multiversion::ProtocolGate* protocolGate = nullptr;

    std::map<std::string, world::LoadedWorld*, std::less<>> worlds;
    std::string defaultWorldName = "world";
    std::int64_t worldSeed = 0;
    std::int32_t maxPlayers = 20;
    std::int32_t port = 25565;
    wire::text::LangTable lang;

    std::function<server::plugins::PluginEventBus*()> pluginEvents;
    std::function<void()> stopServer;
    /// parity: a block a command knocks down drops its own items.
    std::function<void(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t destroyedState)>
        dropBrokenBlock;
    /// parity: `CarvedPumpkinBlock.trySpawnGolem`.
    std::function<void(world::LoadedWorld& world, std::string_view type, double x, double y, double z)> summonGolem;
    std::function<bool(world::LoadedWorld& world, entity::ServerEntity& entity)> killOutright;
    std::function<void()> requestRestart;
    std::function<void()> onConfigsReloaded;
    std::function<std::int64_t()> nowNanos;
    std::function<std::int64_t()> nowMillis;
    std::function<std::int64_t()> nowEpochSeconds;
    std::function<double()> nextDouble;
    std::function<std::int64_t()> nextSeed;
    std::function<std::optional<std::string>(const std::string& name)> fetchMojangUuid;
    std::function<bool()> whitelistEnforced;
    std::function<void(bool)> setWhitelistEnforced;
    std::function<void(std::int32_t minutes)> setIdleTimeout;
};

struct WiredCommands {
    std::unique_ptr<command::CommandEnvironment> environment;
    std::unique_ptr<command::DataAccess> data;
    std::unique_ptr<command::CommandDispatcher> dispatcher;
};

[[nodiscard]] WiredCommands buildCommands(const CommandWiring& wiring);

} // namespace basalt::server::runtime
