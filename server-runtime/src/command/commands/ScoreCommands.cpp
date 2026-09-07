#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/ScoreCommands.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::text::Component;

/// parity: `GameRule.getCommandResult`, which is the integer itself, or 0/1 for a boolean.
[[nodiscard]] std::int32_t commandResult(std::string_view value) {
    std::int32_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) {
        return number;
    }
    return value == "true" ? 1 : 0;
}

[[nodiscard]] Parsed<std::int32_t> setRule(const CommandContext& context, const world::GameRule& rule,
                                           const std::string& value) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    (*world)->gameRules().set(rule, value);
    context.success(Component::translatable("commands.gamerule.set",
                                            {Component::text(std::string{rule.id}), Component::text(value)}),
                    true);
    return commandResult(value);
}

} // namespace

GameRuleCommand::GameRuleCommand() : Command("gamerule", permissionLevel::kGamemasters) {}

void GameRuleCommand::build(const CommandNodeBuilder& root) const {
    for (const world::GameRule& rule : world::gameRules::all()) {
        for (const std::string& name : {std::string{rule.id}, "minecraft:" + std::string{rule.id}}) {
            root.literal(name, [&rule](const CommandNodeBuilder& form) {
                form.executes([&rule](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    const std::string value = (*world)->gameRules().serialized(rule);
                    context.success(Component::translatable(
                        "commands.gamerule.query", {Component::text(std::string{rule.id}), Component::text(value)}));
                    return commandResult(value);
                });
                if (rule.kind == world::GameRule::Kind::Boolean) {
                    form.argument("value", boolArgument(), [&rule](const CommandNodeBuilder& value) {
                        value.executes([&rule](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto* flag = context.get<bool>("value");
                            return flag == nullptr ? 0 : setRule(context, rule, *flag ? "true" : "false");
                        });
                    });
                } else {
                    form.argument(
                        "value", integerArgument(rule.min, rule.max), [&rule](const CommandNodeBuilder& value) {
                            value.executes([&rule](const CommandContext& context) -> Parsed<std::int32_t> {
                                const auto* number = context.get<std::int32_t>("value");
                                return number == nullptr ? 0 : setRule(context, rule, std::to_string(*number));
                            });
                        });
                }
            });
        }
    }
}

TriggerCommand::TriggerCommand(scoreboard::Scoreboard& scoreboard)
    : Command("trigger", permissionLevel::kAll), scoreboard_(&scoreboard) {}

void TriggerCommand::build(const CommandNodeBuilder& root) const {
    // parity: the bare form adds one, `add` adds what it was given, `set` replaces.
    const auto run = [this](const CommandContext& context, std::string_view verb,
                            const std::function<std::int32_t(std::int32_t)>& newValue) -> Parsed<std::int32_t> {
        const Parsed<player::PlayerSession*> player = context.requirePlayer();
        if (!player.has_value()) {
            return std::unexpected{player.error()};
        }
        const auto* name = context.get<std::string>("objective");
        const scoreboard::Objective* objective = name == nullptr ? nullptr : scoreboard_->get(*name);
        if (objective == nullptr) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.trigger.failed.unprimed")}};
        }
        if (!objective->isTrigger()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.trigger.failed.invalid")}};
        }
        if (!scoreboard_->isTriggerEnabled((*player)->name(), objective->name)) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.trigger.failed.unprimed")}};
        }
        const std::int32_t value = newValue(scoreboard_->score((*player)->name(), objective->name).value_or(0));
        scoreboard_->setScore((*player)->name(), objective->name, value);
        // Vanilla disarms it here, so a menu cannot be spammed without being re-enabled.
        scoreboard_->disableTrigger((*player)->name(), objective->name);
        // parity: the bare form reports only the objective; add and set also report the value.
        context.success(
            verb == "simple"
                ? Component::translatable("commands.trigger.simple.success", {Component::text(objective->name)})
                : Component::translatable("commands.trigger." + std::string{verb} + ".success",
                                          {Component::text(objective->name), Component::text(std::to_string(value))}));
        return value;
    };

    root.argument("objective", objectiveArgument(), [this, run](const CommandNodeBuilder& objective) {
        objective.suggestsFromServer([this](const SuggestionContext& context) {
            std::vector<std::string> armed;
            for (const scoreboard::Objective* candidate : scoreboard_->allObjectives()) {
                if (candidate->isTrigger()) {
                    armed.push_back(candidate->name);
                }
            }
            return context.matching(armed);
        });
        objective.executes([run](const CommandContext& context) {
            return run(context, "simple", [](std::int32_t current) { return current + 1; });
        });
        objective.literal("add", [run](const CommandNodeBuilder& add) {
            add.argument("value", integerArgument(), [run](const CommandNodeBuilder& value) {
                value.executes([run](const CommandContext& context) {
                    const auto* by = context.get<std::int32_t>("value");
                    return run(context, "add",
                               [by](std::int32_t current) { return by == nullptr ? current : current + *by; });
                });
            });
        });
        objective.literal("set", [run](const CommandNodeBuilder& set) {
            set.argument("value", integerArgument(), [run](const CommandNodeBuilder& value) {
                value.executes([run](const CommandContext& context) {
                    const auto* to = context.get<std::int32_t>("value");
                    return run(context, "set", [to](std::int32_t current) { return to == nullptr ? current : *to; });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
