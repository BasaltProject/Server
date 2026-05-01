#include <basalt/server/runtime/command/VanillaCommands.hpp>
#include <basalt/server/runtime/command/commands/AccessCommands.hpp>
#include <basalt/server/runtime/command/commands/BasicCommands.hpp>
#include <basalt/server/runtime/command/commands/BlockCommands.hpp>
#include <basalt/server/runtime/command/commands/BossBarCommand.hpp>
#include <basalt/server/runtime/command/commands/ComputeCommand.hpp>
#include <basalt/server/runtime/command/commands/EffectCommand.hpp>
#include <basalt/server/runtime/command/commands/EntityCommands.hpp>
#include <basalt/server/runtime/command/commands/GroupCommands.hpp>
#include <basalt/server/runtime/command/commands/HudCommands.hpp>
#include <basalt/server/runtime/command/commands/ImportPlayersCommand.hpp>
#include <basalt/server/runtime/command/commands/ItemCommands.hpp>
#include <basalt/server/runtime/command/commands/LocateCommand.hpp>
#include <basalt/server/runtime/command/commands/ParticleCommand.hpp>
#include <basalt/server/runtime/command/commands/PlaceholderCommand.hpp>
#include <basalt/server/runtime/command/commands/PlayerCommands.hpp>
#include <basalt/server/runtime/command/commands/PostEffectCommand.hpp>
#include <basalt/server/runtime/command/commands/ProgressCommands.hpp>
#include <basalt/server/runtime/command/commands/RandomCommand.hpp>
#include <basalt/server/runtime/command/commands/RegionCommands.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/command/commands/ScoreCommands.hpp>
#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>
#include <basalt/server/runtime/command/commands/SessionCommands.hpp>
#include <basalt/server/runtime/command/commands/StopwatchCommand.hpp>
#include <basalt/server/runtime/command/commands/TagCommand.hpp>
#include <basalt/server/runtime/command/commands/TeamCommand.hpp>
#include <basalt/server/runtime/command/commands/TeleportCommand.hpp>
#include <basalt/server/runtime/command/commands/TimeCommand.hpp>
#include <basalt/server/runtime/command/commands/WorldBorderCommand.hpp>
#include <basalt/server/runtime/command/commands/WorldCommands.hpp>

#include <cstdint>

namespace basalt::server::runtime::command {
namespace {

struct OwnedState {
    commands::Stopwatches stopwatches;
    commands::BossBarRegistry bossBars;
};

[[nodiscard]] OwnedState& ownedState(const std::function<std::int64_t()>& nowMillis) {
    static OwnedState state{commands::Stopwatches{nowMillis}, commands::BossBarRegistry{}};
    return state;
}

} // namespace

std::vector<std::shared_ptr<const Command>> vanillaCommands(const VanillaCommandHooks& hooks) {
    std::vector<std::shared_ptr<const Command>> built;
    const auto add = [&built](std::shared_ptr<const Command> command) { built.push_back(std::move(command)); };

    add(std::make_shared<commands::HelpCommand>());
    add(std::make_shared<commands::ListPlayersCommand>());
    add(std::make_shared<commands::PlaceholderCommand>());
    add(std::make_shared<commands::SeedCommand>());
    add(std::make_shared<commands::SayCommand>());
    add(std::make_shared<commands::EmoteCommand>());
    add(std::make_shared<commands::VersionCommand>());
    add(std::make_shared<commands::GameModeCommand>());
    add(std::make_shared<commands::DefaultGameModeCommand>());
    add(std::make_shared<commands::GiveCommand>());
    add(std::make_shared<commands::ClearCommand>());
    add(std::make_shared<commands::TeleportCommand>());
    add(std::make_shared<commands::WorldCommand>());
    add(std::make_shared<commands::SetBlockCommand>());
    add(std::make_shared<commands::FillCommand>());
    add(std::make_shared<commands::CloneCommand>());
    add(std::make_shared<commands::FillBiomeCommand>());
    add(std::make_shared<commands::ForceLoadCommand>());
    add(std::make_shared<commands::LocateCommand>());
    add(std::make_shared<commands::GameRuleCommand>());
    add(std::make_shared<commands::TimeCommand>());
    add(std::make_shared<commands::WeatherCommand>());
    add(std::make_shared<commands::DifficultyCommand>());
    add(std::make_shared<commands::SetWorldSpawnCommand>());
    add(std::make_shared<commands::SpawnPointCommand>());
    add(std::make_shared<commands::WorldBorderCommand>());
    add(std::make_shared<commands::TellRawCommand>());
    add(std::make_shared<commands::MsgCommand>());
    add(std::make_shared<commands::TitleCommand>());
    add(std::make_shared<commands::TagCommand>());
    add(std::make_shared<commands::PostEffectCommand>());
    add(std::make_shared<commands::EffectCommand>());
    add(std::make_shared<commands::ParticleCommand>());
    add(std::make_shared<commands::StopSoundCommand>());
    add(std::make_shared<commands::KillCommand>());
    add(std::make_shared<commands::KickCommand>());
    add(std::make_shared<commands::ExperienceCommand>());
    add(std::make_shared<commands::SpectateCommand>());
    add(std::make_shared<commands::TransferCommand>());
    add(std::make_shared<commands::OpCommand>());
    add(std::make_shared<commands::DeopCommand>());
    add(std::make_shared<commands::GroupCommand>());
    add(std::make_shared<commands::BanListCommand>());
    add(std::make_shared<commands::PardonCommand>(false));
    add(std::make_shared<commands::PardonCommand>(true));
    add(std::make_shared<commands::RideCommand>());
    add(std::make_shared<commands::RotateCommand>());
    add(std::make_shared<commands::SwingCommand>());
    add(std::make_shared<commands::AttributeCommand>());
    add(std::make_shared<commands::DamageCommand>());
    add(std::make_shared<commands::StopCommand>());

    if (hooks.players != nullptr) {
        add(std::make_shared<commands::SummonCommand>(*hooks.players));
    }
    if (hooks.scoreboard != nullptr) {
        add(std::make_shared<commands::ScoreboardCommand>(*hooks.scoreboard));
        add(std::make_shared<commands::TriggerCommand>(*hooks.scoreboard));
    }
    if (hooks.teams != nullptr) {
        add(std::make_shared<commands::TeamCommand>(*hooks.teams));
        add(std::make_shared<commands::TeamMsgCommand>(*hooks.teams));
    }
    if (hooks.advancements != nullptr && hooks.progress != nullptr) {
        add(std::make_shared<commands::AdvancementCommand>(*hooks.advancements, *hooks.progress));
    }
    if (hooks.progress != nullptr && hooks.recipes != nullptr) {
        add(std::make_shared<commands::RecipeCommand>(*hooks.progress, *hooks.recipes));
    }
    add(std::make_shared<commands::WaypointCommand>());
    add(std::make_shared<commands::ItemCommand>(hooks.recipes, hooks.lootTables));
    add(std::make_shared<commands::ComputeCommand>(hooks.lootTables));
    if (hooks.lootTables != nullptr && hooks.worldSeed) {
        add(std::make_shared<commands::LootCommand>(*hooks.lootTables));
    }
    if (hooks.templates != nullptr && hooks.nextStructureSeed) {
        add(std::make_shared<commands::PlaceCommand>(*hooks.templates, hooks.nextStructureSeed));
    }
    if (hooks.enchantments != nullptr) {
        add(std::make_shared<commands::EnchantCommand>(*hooks.enchantments));
    }
    if (hooks.dialogs != nullptr) {
        add(std::make_shared<commands::DialogCommand>(*hooks.dialogs));
    }
    if (hooks.testInstances != nullptr && hooks.functionRunner != nullptr) {
        add(std::make_shared<commands::TestCommand>(*hooks.testInstances, *hooks.functionRunner));
    }
    if (hooks.worldSeed && hooks.randomSequences) {
        add(std::make_shared<commands::RandomCommand>(hooks.worldSeed, hooks.randomSequences));
    }
    if (hooks.nowMillis) {
        add(std::make_shared<commands::StopwatchCommand>(ownedState(hooks.nowMillis).stopwatches));
    }
    add(std::make_shared<commands::BossBarCommand>(ownedState(hooks.nowMillis).bossBars));
    if (hooks.nextDouble) {
        add(std::make_shared<commands::SpreadPlayersCommand>(hooks.nextDouble));
    }
    if (hooks.nextSoundSeed) {
        add(std::make_shared<commands::PlaySoundCommand>(hooks.nextSoundSeed));
    }
    if (hooks.setIdleTimeout) {
        add(std::make_shared<commands::SetIdleTimeoutCommand>(hooks.setIdleTimeout));
    }
    if (hooks.nowEpochSeconds) {
        add(std::make_shared<commands::ImportPlayersCommand>(hooks.nowEpochSeconds));
        add(std::make_shared<commands::BanCommand>(hooks.nowEpochSeconds));
        add(std::make_shared<commands::BanIpCommand>(hooks.nowEpochSeconds));
        if (hooks.whitelistState && hooks.setWhitelistState) {
            add(std::make_shared<commands::WhitelistCommand>(hooks.whitelistState, hooks.setWhitelistState,
                                                             hooks.nowEpochSeconds));
        }
    }
    return built;
}

void registerVanillaCommands(CommandDispatcher& dispatcher, const VanillaCommandHooks& hooks) {
    for (const std::shared_ptr<const Command>& command : vanillaCommands(hooks)) {
        static_cast<void>(dispatcher.registerCommand(command));
    }
}

} // namespace basalt::server::runtime::command
