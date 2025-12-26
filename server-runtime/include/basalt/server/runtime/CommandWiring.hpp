#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/command/commands/FunctionCommands.hpp>
#include <basalt/server/runtime/command/commands/SaveCommands.hpp>
#include <basalt/server/runtime/command/commands/StopwatchCommand.hpp>
#include <basalt/server/storage/PlayerStore.hpp>
#include <basalt/wire/common/timings/TickDurations.hpp>
#include <basalt/wire/multiversion/ProtocolGate.hpp>

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

/// Everything the command tree is built out of that only a running server owns.
///
/// A struct rather than forty parameters: almost all of it is a pointer the server already holds,
/// and a caller that has none of it (a test) leaves them null and gets the commands that need
/// nothing.
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

/// The tree and the two things it borrows for as long as it lives.
///
/// The dispatcher holds a reference to its environment and the commands hold one to the data access,
/// so all three travel together: a caller that kept only the dispatcher would be holding a dangling
/// reference the moment this went out of scope.
struct WiredCommands {
    std::unique_ptr<command::CommandEnvironment> environment;
    std::unique_ptr<command::DataAccess> data;
    std::unique_ptr<command::CommandDispatcher> dispatcher;
};

/// Builds and registers every vanilla command, plus the ones tied to server-wide state.
///
/// Pure construction-time wiring: nothing here runs per tick. Kept out of the server's own
/// constructor so that class stays about what a server *is*, not about assembling a command tree.
[[nodiscard]] WiredCommands buildCommands(const CommandWiring& wiring);

} // namespace basalt::server::runtime
