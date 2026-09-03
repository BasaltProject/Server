#include "ScoreboardCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using scoreboard::Objective;
using scoreboardCommand::broadcast;
using scoreboardCommand::holdersOf;
using scoreboardCommand::joined;
using scoreboardCommand::numberFormats;
using scoreboardCommand::objectiveOf;
using scoreboardCommand::sendScore;
using wire::text::Component;

/// parity: the one report `set`, `add` and `remove` share, which names the amount and the objective.
void report(const CommandContext& context, const std::vector<std::string>& holders, const Objective& objective,
            const std::string& action, std::int32_t value) {
    context.success(holders.size() == 1
                        ? Component::translatable("commands.scoreboard.players." + action + ".success.single",
                                                  {Component::text(std::to_string(value)), objective.displayName,
                                                   Component::text(holders.front())})
                        : Component::translatable("commands.scoreboard.players." + action + ".success.multiple",
                                                  {Component::text(std::to_string(value)), objective.displayName,
                                                   Component::text(std::to_string(holders.size()))}),
                    true);
}

/// parity: `OperationArgument`'s own nine, including the two that refuse a zero divisor.
[[nodiscard]] std::expected<std::int32_t, CommandSyntaxError>
applyOperation(std::string_view operation, std::int32_t current, std::int32_t source) {
    if (operation == "=") {
        return source;
    }
    if (operation == "+=") {
        return current + source;
    }
    if (operation == "-=") {
        return current - source;
    }
    if (operation == "*=") {
        return current * source;
    }
    if (operation == "<") {
        return std::min(current, source);
    }
    if (operation == ">") {
        return std::max(current, source);
    }
    if (operation == "/=" || operation == "%=") {
        if (source == 0) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("arguments.operation.div0")}};
        }
        if (operation == "/=") {
            // parity: vanilla floors, so -7 / 2 is -4, not -3.
            return static_cast<std::int32_t>(std::floor(static_cast<double>(current) / static_cast<double>(source)));
        }
        // parity: `Math.floorMod`, whose answer takes the divisor's sign rather than the dividend's.
        const std::int32_t remainder = current % source;
        return remainder != 0 && ((remainder < 0) != (source < 0)) ? remainder + source : remainder;
    }
    return std::unexpected{CommandSyntaxError{.component = Component::translatable("arguments.operation.invalid")}};
}

} // namespace

void ScoreboardCommand::buildPlayers(const CommandNodeBuilder& root) const {
    root.literal("list", [this](const CommandNodeBuilder& list) {
        list.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::vector<std::string> holders = scoreboard_->holders();
            context.success(holders.empty() ? Component::translatable("commands.scoreboard.players.list.empty")
                                            : Component::translatable("commands.scoreboard.players.list.success",
                                                                      {Component::text(std::to_string(holders.size())),
                                                                       Component::text(joined(holders))}));
            return static_cast<std::int32_t>(holders.size());
        });
        list.argument("target", scoreHolderArgument(), [this](const CommandNodeBuilder& target) {
            target.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const std::vector<std::string>& named = holdersOf(context, "target");
                if (named.empty()) {
                    return 0;
                }
                const std::vector<std::pair<std::string, std::int32_t>> scores = scoreboard_->scoresOf(named.front());
                context.success(scores.empty()
                                    ? Component::translatable("commands.scoreboard.players.list.entity.empty",
                                                              {Component::text(named.front())})
                                    : Component::translatable("commands.scoreboard.players.list.entity.success",
                                                              {Component::text(named.front()),
                                                               Component::text(std::to_string(scores.size()))}));
                return static_cast<std::int32_t>(scores.size());
            });
        });
    });

    root.literal("get", [this](const CommandNodeBuilder& form) {
        form.argument("target", scoreHolderArgument(), [this](const CommandNodeBuilder& target) {
            target.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
                named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto objective = objectiveOf(*scoreboard_, context);
                    if (!objective.has_value()) {
                        return std::unexpected{objective.error()};
                    }
                    const std::vector<std::string>& holders = holdersOf(context, "target");
                    const std::optional<std::int32_t> score =
                        holders.empty() ? std::nullopt : scoreboard_->score(holders.front(), (*objective)->name);
                    if (!score.has_value()) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable(
                                "commands.scoreboard.players.get.null",
                                {(*objective)->displayName,
                                 Component::text(holders.empty() ? std::string{} : holders.front())})}};
                    }
                    context.success(
                        Component::translatable("commands.scoreboard.players.get.success",
                                                {Component::text(holders.front()),
                                                 Component::text(std::to_string(*score)), (*objective)->displayName}));
                    return *score;
                });
            });
        });
    });

    root.literal("display", [this](const CommandNodeBuilder& display) {
        /// The per-holder overrides, which differ only in what they set and what they then say.
        const auto resendScore = [this](const CommandContext& context, const std::string& holder,
                                        const Objective& objective) {
            const std::optional<std::int32_t> value = scoreboard_->score(holder, objective.name);
            if (!value.has_value()) {
                return;
            }
            const Component* name = scoreboard_->displayNameOf(holder, objective.name);
            const play::NumberFormat* format = scoreboard_->numberFormatOf(holder, objective.name);
            broadcast(context,
                      play::SetScorePacket{.owner = holder,
                                           .objectiveName = objective.name,
                                           .score = *value,
                                           .display = name == nullptr ? std::nullopt : std::optional{*name},
                                           .numberFormat = format == nullptr ? std::nullopt : std::optional{*format}});
        };
        display.literal("name", [this, resendScore](const CommandNodeBuilder& form) {
            form.argument("targets", scoreHolderArgument(), [this, resendScore](const CommandNodeBuilder& targets) {
                targets.argument(
                    "objective", objectiveArgument(), [this, resendScore](const CommandNodeBuilder& named) {
                        const auto set = [this, resendScore](const CommandContext& context,
                                                             bool given) -> Parsed<std::int32_t> {
                            const auto objective = objectiveOf(*scoreboard_, context);
                            if (!objective.has_value()) {
                                return std::unexpected{objective.error()};
                            }
                            const auto* label = given ? context.get<Component>("name") : nullptr;
                            const std::vector<std::string>& holders = holdersOf(context, "targets");
                            for (const std::string& holder : holders) {
                                scoreboard_->setDisplayName(holder, (*objective)->name,
                                                            label == nullptr ? std::nullopt : std::optional{*label});
                                resendScore(context, holder, **objective);
                            }
                            context.success(Component::translatable(
                                                label == nullptr
                                                    ? "commands.scoreboard.players.display.name.clear.success.single"
                                                    : "commands.scoreboard.players.display.name.success.single",
                                                {Component::text(holders.empty() ? std::string{} : holders.front()),
                                                 (*objective)->displayName}),
                                            true);
                            return static_cast<std::int32_t>(holders.size());
                        };
                        named.executes([set](const CommandContext& context) { return set(context, false); });
                        named.argument("name", componentArgument(), [set](const CommandNodeBuilder& label) {
                            label.executes([set](const CommandContext& context) { return set(context, true); });
                        });
                    });
            });
        });
        display.literal("numberformat", [this, resendScore](const CommandNodeBuilder& form) {
            form.argument("targets", scoreHolderArgument(), [this, resendScore](const CommandNodeBuilder& targets) {
                targets.argument(
                    "objective", objectiveArgument(), [this, resendScore](const CommandNodeBuilder& named) {
                        const auto set =
                            [this, resendScore](const CommandContext& context,
                                                std::optional<play::NumberFormat> format) -> Parsed<std::int32_t> {
                            const auto objective = objectiveOf(*scoreboard_, context);
                            if (!objective.has_value()) {
                                return std::unexpected{objective.error()};
                            }
                            const std::vector<std::string>& holders = holdersOf(context, "targets");
                            for (const std::string& holder : holders) {
                                scoreboard_->setNumberFormat(holder, (*objective)->name, format);
                                resendScore(context, holder, **objective);
                            }
                            context.success(
                                Component::translatable(
                                    format.has_value()
                                        ? "commands.scoreboard.players.display.numberFormat.success.single"
                                        : "commands.scoreboard.players.display.numberFormat.clear.success.single",
                                    {Component::text(holders.empty() ? std::string{} : holders.front()),
                                     (*objective)->displayName}),
                                true);
                            return static_cast<std::int32_t>(holders.size());
                        };
                        named.executes([set](const CommandContext& context) { return set(context, std::nullopt); });
                        numberFormats(named, set);
                    });
            });
        });
    });

    root.literal("set", [this](const CommandNodeBuilder& form) {
        form.argument("targets", scoreHolderArgument(), [this](const CommandNodeBuilder& targets) {
            targets.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
                named.argument("score", integerArgument(), [this](const CommandNodeBuilder& score) {
                    score.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto objective = objectiveOf(*scoreboard_, context);
                        if (!objective.has_value()) {
                            return std::unexpected{objective.error()};
                        }
                        const auto* value = context.get<std::int32_t>("score");
                        if (value == nullptr) {
                            return 0;
                        }
                        const std::vector<std::string>& holders = holdersOf(context, "targets");
                        for (const std::string& holder : holders) {
                            scoreboard_->setScore(holder, (*objective)->name, *value);
                            sendScore(*scoreboard_, context, holder, (*objective)->name, *value);
                        }
                        report(context, holders, **objective, "set", *value);
                        return *value * static_cast<std::int32_t>(holders.size());
                    });
                });
            });
        });
    });

    /// `add` and `remove` are the same walk with opposite signs.
    for (const bool adding : {true, false}) {
        root.literal(adding ? "add" : "remove", [this, adding](const CommandNodeBuilder& form) {
            form.argument("targets", scoreHolderArgument(), [this, adding](const CommandNodeBuilder& targets) {
                targets.argument("objective", objectiveArgument(), [this, adding](const CommandNodeBuilder& named) {
                    named.argument("score", integerArgument(0), [this, adding](const CommandNodeBuilder& score) {
                        score.executes([this, adding](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto objective = objectiveOf(*scoreboard_, context);
                            if (!objective.has_value()) {
                                return std::unexpected{objective.error()};
                            }
                            const auto* asked = context.get<std::int32_t>("score");
                            if (asked == nullptr) {
                                return 0;
                            }
                            const std::int32_t delta = adding ? *asked : -*asked;
                            const std::vector<std::string>& holders = holdersOf(context, "targets");
                            std::int32_t total = 0;
                            for (const std::string& holder : holders) {
                                const std::int32_t value = scoreboard_->addScore(holder, (*objective)->name, delta);
                                sendScore(*scoreboard_, context, holder, (*objective)->name, value);
                                total += value;
                            }
                            report(context, holders, **objective, adding ? "add" : "remove", *asked);
                            return total;
                        });
                    });
                });
            });
        });
    }

    root.literal("reset", [this](const CommandNodeBuilder& form) {
        const auto reset = [this](const CommandContext& context, bool named) -> Parsed<std::int32_t> {
            std::optional<std::string> objectiveName;
            if (named) {
                const auto objective = objectiveOf(*scoreboard_, context);
                if (!objective.has_value()) {
                    return std::unexpected{objective.error()};
                }
                objectiveName = (*objective)->name;
            }
            const std::vector<std::string>& holders = holdersOf(context, "targets");
            for (const std::string& holder : holders) {
                scoreboard_->resetScore(holder, objectiveName);
                broadcast(context, play::ResetScorePacket{.owner = holder, .objectiveName = objectiveName});
            }
            const std::string key = named ? "specific" : "all";
            context.success(holders.size() == 1
                                ? Component::translatable("commands.scoreboard.players.reset." + key + ".single",
                                                          {Component::text(holders.front())})
                                : Component::translatable("commands.scoreboard.players.reset." + key + ".multiple",
                                                          {Component::text(std::to_string(holders.size()))}),
                            true);
            return static_cast<std::int32_t>(holders.size());
        };
        form.argument("targets", scoreHolderArgument(), [reset](const CommandNodeBuilder& targets) {
            targets.executes([reset](const CommandContext& context) { return reset(context, false); });
            targets.argument("objective", objectiveArgument(), [reset](const CommandNodeBuilder& named) {
                named.executes([reset](const CommandContext& context) { return reset(context, true); });
            });
        });
    });

    root.literal("enable", [this](const CommandNodeBuilder& form) {
        form.argument("targets", scoreHolderArgument(), [this](const CommandNodeBuilder& targets) {
            targets.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
                named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto objective = objectiveOf(*scoreboard_, context);
                    if (!objective.has_value()) {
                        return std::unexpected{objective.error()};
                    }
                    if (!(*objective)->isTrigger()) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.scoreboard.players.enable.invalid")}};
                    }
                    const std::vector<std::string>& holders = holdersOf(context, "targets");
                    for (const std::string& holder : holders) {
                        scoreboard_->enableTrigger(holder, (*objective)->name);
                        if (!scoreboard_->score(holder, (*objective)->name).has_value()) {
                            scoreboard_->setScore(holder, (*objective)->name, 0);
                            sendScore(*scoreboard_, context, holder, (*objective)->name, 0);
                        }
                    }
                    context.success(
                        holders.size() == 1
                            ? Component::translatable("commands.scoreboard.players.enable.success.single",
                                                      {(*objective)->displayName, Component::text(holders.front())})
                            : Component::translatable(
                                  "commands.scoreboard.players.enable.success.multiple",
                                  {(*objective)->displayName, Component::text(std::to_string(holders.size()))}),
                        true);
                    return static_cast<std::int32_t>(holders.size());
                });
            });
        });
    });

    root.literal("operation", [this](const CommandNodeBuilder& form) {
        form.argument("targets", scoreHolderArgument(), [this](const CommandNodeBuilder& targets) {
            targets.argument("targetObjective", objectiveArgument(), [this](const CommandNodeBuilder& into) {
                into.argument("operation", operationArgument(), [this](const CommandNodeBuilder& operation) {
                    operation.argument("source", scoreHolderArgument(), [this](const CommandNodeBuilder& source) {
                        source.argument("sourceObjective", objectiveArgument(), [this](const CommandNodeBuilder& from) {
                            from.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                                const auto target = objectiveOf(*scoreboard_, context, "targetObjective");
                                if (!target.has_value()) {
                                    return std::unexpected{target.error()};
                                }
                                const auto sourceObjective = objectiveOf(*scoreboard_, context, "sourceObjective");
                                if (!sourceObjective.has_value()) {
                                    return std::unexpected{sourceObjective.error()};
                                }
                                const auto* operation = context.get<std::string>("operation");
                                const std::vector<std::string>& sourceHolders = holdersOf(context, "source");
                                if (operation == nullptr || sourceHolders.empty()) {
                                    return 0;
                                }
                                const std::string& sourceHolder = sourceHolders.front();
                                const std::int32_t sourceValue =
                                    scoreboard_->score(sourceHolder, (*sourceObjective)->name).value_or(0);

                                const std::vector<std::string>& holders = holdersOf(context, "targets");
                                std::int32_t total = 0;
                                for (const std::string& holder : holders) {
                                    const std::int32_t current =
                                        scoreboard_->score(holder, (*target)->name).value_or(0);
                                    std::int32_t result = 0;
                                    if (*operation == "><") {
                                        // The swap writes the source too, which no other operation
                                        // does.
                                        scoreboard_->setScore(sourceHolder, (*sourceObjective)->name, current);
                                        sendScore(*scoreboard_, context, sourceHolder, (*sourceObjective)->name,
                                                  current);
                                        result = sourceValue;
                                    } else {
                                        const auto applied = applyOperation(*operation, current, sourceValue);
                                        if (!applied.has_value()) {
                                            return std::unexpected{applied.error()};
                                        }
                                        result = *applied;
                                    }
                                    scoreboard_->setScore(holder, (*target)->name, result);
                                    sendScore(*scoreboard_, context, holder, (*target)->name, result);
                                    total += result;
                                }
                                context.success(
                                    holders.size() == 1
                                        ? Component::translatable(
                                              "commands.scoreboard.players.operation.success.single",
                                              {(*target)->displayName, Component::text(holders.front()),
                                               Component::text(std::to_string(
                                                   scoreboard_->score(holders.front(), (*target)->name).value_or(0)))})
                                        : Component::translatable(
                                              "commands.scoreboard.players.operation.success.multiple",
                                              {(*target)->displayName,
                                               Component::text(std::to_string(holders.size()))}),
                                    true);
                                return total;
                            });
                        });
                    });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
