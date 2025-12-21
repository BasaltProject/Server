#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

#include <memory>
#include <vector>

namespace basalt::server::runtime::command {

/// What the commands that need something only the running server owns are handed.
///
/// Every field is optional: a command whose dependency is absent is simply not registered, which is
/// how a test fixture registers the twenty-odd that need nothing. The alternative, registering it
/// against a null and failing when someone runs it, moves the gap from startup to the moment an
/// operator needs the command.
struct VanillaCommandHooks {
    player::PlayerRegistry* players = nullptr;
    scoreboard::Scoreboard* scoreboard = nullptr;
    scoreboard::Teams* teams = nullptr;
    world::RealAdvancements* advancements = nullptr;
    world::PlayerProgress* progress = nullptr;
    world::RealRecipes* recipes = nullptr;

    /// The world seed a `/random` draws from when it was given no sequence.
    std::function<std::int64_t()> worldSeed;
    /// Wall-clock milliseconds, for `/stopwatch`.
    std::function<std::int64_t()> nowMillis;
    /// Seconds since the epoch, for the bans and the whitelist.
    std::function<std::int64_t()> nowEpochSeconds;
    /// A fresh draw in [0, 1), which is what `/spreadplayers` scatters with.
    std::function<double()> nextDouble;
    /// A seed per sound, so two `/playsound` of the same sound do not sound identical.
    std::function<std::int64_t()> nextSoundSeed;
    /// What `/setidletimeout` records.
    std::function<void(std::int32_t minutes)> setIdleTimeout;

    bool whitelistEnforced = false;
    std::function<bool()> whitelistState;
    std::function<void(bool)> setWhitelistState;
};

/// Every command Basalt understands, in registration order, one class each under
/// `command/commands/` as vanilla splits them. Each reimplements its counterpart's tree shape,
/// permission level and translation keys: the client resolves the keys, so a different one is
/// visible.
///
/// Gaps, declared rather than silent: the data, datapack, execute, function, item and loot
/// families. `import-players`, `placeholder`, `world` and `reload` have no vanilla counterpart.
/// `reload`, `restart`, `timings` and the profiling family are registered by the server itself
/// instead, each needing something only a running server owns.
[[nodiscard]] std::vector<std::shared_ptr<const Command>> vanillaCommands(const VanillaCommandHooks& hooks);

void registerVanillaCommands(CommandDispatcher& dispatcher, const VanillaCommandHooks& hooks);

} // namespace basalt::server::runtime::command
