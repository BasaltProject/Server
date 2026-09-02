#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/commands/PlayerCommands.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `DamageTypes.GENERIC_KILL`'s message id, which the client renders as "<name> died".
constexpr std::string_view kDeathGenericKill = "genericKill";

[[nodiscard]] std::string modeName(Gamemode mode) {
    switch (mode) {
    case Gamemode::Creative:
        return "creative";
    case Gamemode::Adventure:
        return "adventure";
    case Gamemode::Spectator:
        return "spectator";
    case Gamemode::Survival:
    default:
        return "survival";
    }
}

/// parity: `GameModeCommand.logGamemodeChange`, including who is told what. Changing your own mode
/// reports once to you; changing someone else's tells them separately with `gameMode.changed` and
/// reports to you with their name.
[[nodiscard]] std::int32_t setMode(const CommandContext& context, const std::vector<player::PlayerSession*>& targets,
                                   Gamemode mode) {
    std::int32_t changed = 0;
    const Component named = Component::translatable("gameMode." + modeName(mode));
    for (player::PlayerSession* target : targets) {
        if (target->gamemode == mode) {
            continue;
        }
        if (target->connection().setGamemode) {
            target->connection().setGamemode(mode);
        }
        changed++;
        const player::PlayerSession* runner = context.source().player();
        if (runner != nullptr && target->uuid() == runner->uuid()) {
            context.success(Component::translatable("commands.gamemode.success.self", {named}), true);
        } else {
            target->sendSystemMessage(Component::translatable("gameMode.changed", {named}));
            context.success(
                Component::translatable("commands.gamemode.success.other", {Component::text(target->name()), named}),
                true);
        }
    }
    return changed;
}

} // namespace

GameModeCommand::GameModeCommand() : Command("gamemode", permissionLevel::kGamemasters) {}

void GameModeCommand::build(const CommandNodeBuilder& root) const {
    root.argument("gamemode", gameModeArgument(), [](const CommandNodeBuilder& gamemode) {
        gamemode.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<player::PlayerSession*> self = context.requirePlayer();
            if (!self.has_value()) {
                return std::unexpected{self.error()};
            }
            const Gamemode* mode = context.get<Gamemode>("gamemode");
            return mode == nullptr ? 0 : setMode(context, {*self}, *mode);
        });
        gamemode.argument("target", playersArgument(false), [](const CommandNodeBuilder& target) {
            target.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* targets = context.get<std::vector<player::PlayerSession*>>("target");
                const Gamemode* mode = context.get<Gamemode>("gamemode");
                return targets == nullptr || mode == nullptr ? 0 : setMode(context, *targets, *mode);
            });
        });
    });
}

DefaultGameModeCommand::DefaultGameModeCommand() : Command("defaultgamemode", permissionLevel::kGamemasters) {}

void DefaultGameModeCommand::build(const CommandNodeBuilder& root) const {
    root.argument("gamemode", gameModeArgument(), [](const CommandNodeBuilder& gamemode) {
        gamemode.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            const Gamemode* mode = context.get<Gamemode>("gamemode");
            if (mode == nullptr) {
                return 0;
            }
            (*world)->setDefaultGamemode(modeName(*mode));
            context.success(Component::translatable("commands.defaultgamemode.success",
                                                    {Component::translatable("gameMode." + modeName(*mode))}),
                            true);
            return 1;
        });
    });
}

KillCommand::KillCommand() : Command("kill", permissionLevel::kGamemasters) {}

void KillCommand::build(const CommandNodeBuilder& root) const {
    const auto kill = [](const CommandContext& context, const std::vector<CommandTarget>& targets) {
        for (const CommandTarget& target : targets) {
            if (player::PlayerSession* session = target.player(); session != nullptr) {
                if (session->connection().killPlayer) {
                    session->connection().killPlayer(kDeathGenericKill);
                }
                continue;
            }
            if (entity::ServerEntity* entity = target.entity(); entity != nullptr) {
                // parity: `/kill` kills, so what it kills drops what it would have dropped. A
                // summoned entity has no death animation to play, so it simply stops existing.
                entity->killed = true;
                entity->removed = true;
                continue;
            }
            // parity: an item entity is discarded rather than killed: it leaves nothing behind.
            const auto& worlds = context.environment().options().worlds;
            if (const auto found = worlds.find(target.worldName()); found != worlds.end()) {
                static_cast<void>(found->second->state().removeItem(target.entityId()));
            }
        }
        const Component message =
            targets.size() == 1
                ? Component::translatable("commands.kill.success.single", {Component::text(targets.front().name())})
                : Component::translatable("commands.kill.success.multiple",
                                          {Component::text(std::to_string(targets.size()))});
        context.success(message, true);
        return Parsed<std::int32_t>{static_cast<std::int32_t>(targets.size())};
    };
    root.executes([kill](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<player::PlayerSession*> self = context.requireEntity();
        if (!self.has_value()) {
            return std::unexpected{self.error()};
        }
        return kill(context, {CommandTarget{**self}});
    });
    root.argument("targets", entitiesArgument(false), [kill](const CommandNodeBuilder& targets) {
        targets.executes([kill](const CommandContext& context) -> Parsed<std::int32_t> {
            const auto* found = context.get<std::vector<CommandTarget>>("targets");
            return found == nullptr ? 0 : kill(context, *found);
        });
    });
}

HelpCommand::HelpCommand() : Command("help") {}

void HelpCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) {
        const std::vector<CommandNode*> usable = context.dispatcher().usableCommands(context.source());
        for (const CommandNode* node : usable) {
            context.source().sendSystemMessage(Component::text("/" + smartUsage(*node, context.source())));
        }
        return Parsed<std::int32_t>{static_cast<std::int32_t>(usable.size())};
    });
    root.argument("command", greedyStringArgument(), [](const CommandNodeBuilder& command) {
        command.suggestsFromServer([](const SuggestionContext& context) {
            std::vector<std::string> names;
            for (const CommandNode* node : context.dispatcher().usableCommands(context.source())) {
                names.push_back(node->name());
            }
            return context.matching(names);
        });
        command.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            // Greedy, as vanilla declares it, but only the command named by the first word is
            // described: see the class docs for why the rest of the line is not.
            const std::string* line = context.get<std::string>("command");
            const std::string name =
                line == nullptr ? std::string{} : line->substr(0, std::min(line->find(' '), line->size()));
            for (const CommandNode* node : context.dispatcher().usableCommands(context.source())) {
                if (node->name() == name) {
                    context.source().sendSystemMessage(Component::text("/" + smartUsage(*node, context.source())));
                    return 1;
                }
            }
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.help.failed")}};
        });
    });
}

} // namespace basalt::server::runtime::command::commands
