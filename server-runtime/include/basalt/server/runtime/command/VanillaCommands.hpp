#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::server::runtime {
class BlockTests;
}

namespace basalt::server::runtime::world {
class RealDialogs;
class RealEnchantments;
class RealLootTables;
class RealTestInstances;
class StructureTemplates;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::command {

namespace commands {
class FunctionRunner;
}

struct VanillaCommandHooks {
    player::PlayerRegistry* players = nullptr;
    scoreboard::Scoreboard* scoreboard = nullptr;
    scoreboard::Teams* teams = nullptr;
    world::RealAdvancements* advancements = nullptr;
    world::PlayerProgress* progress = nullptr;
    world::RealRecipes* recipes = nullptr;
    world::StructureTemplates* templates = nullptr;
    world::RealEnchantments* enchantments = nullptr;
    world::RealDialogs* dialogs = nullptr;
    world::RealLootTables* lootTables = nullptr;
    world::RealTestInstances* testInstances = nullptr;
    commands::FunctionRunner* functionRunner = nullptr;
    std::function<BlockTests*()> blockTests;

    std::function<std::int64_t()> worldSeed;
    std::function<world::RandomSequences&()> randomSequences;
    std::function<std::int64_t()> nowMillis;
    std::function<std::int64_t()> nowEpochSeconds;
    std::function<double()> nextDouble;
    std::function<std::int64_t()> nextStructureSeed;
    std::function<std::int64_t()> nextSoundSeed;
    std::function<void(std::int32_t minutes)> setIdleTimeout;

    bool whitelistEnforced = false;
    std::function<bool()> whitelistState;
    std::function<void(bool)> setWhitelistState;
    std::function<void()> kickUnlistedPlayers;
};

[[nodiscard]] std::vector<std::shared_ptr<const Command>> vanillaCommands(const VanillaCommandHooks& hooks);

void registerVanillaCommands(CommandDispatcher& dispatcher, const VanillaCommandHooks& hooks);

} // namespace basalt::server::runtime::command
