#include "ScoreboardCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using scoreboard::Objective;
using scoreboardCommand::broadcast;
using scoreboardCommand::joined;
using scoreboardCommand::numberFormats;
using scoreboardCommand::objectiveOf;
using scoreboardCommand::resend;
using wire::text::Component;

/// parity: vanilla's own cap on an objective name.
constexpr std::size_t kMaxNameLength = 32768;

[[nodiscard]] std::string slotName(play::DisplaySlot slot) {
    switch (slot) {
    case play::DisplaySlot::Sidebar:
        return "sidebar";
    case play::DisplaySlot::BelowName:
        return "below_name";
    case play::DisplaySlot::List:
    default:
        return "list";
    }
}

/// parity: `ObjectiveRenderType`'s own lowercase names, which the command's literals are.
[[nodiscard]] std::string renderTypeName(play::ObjectiveRenderType type) {
    return type == play::ObjectiveRenderType::Hearts ? "hearts" : "integer";
}

} // namespace

void ScoreboardCommand::buildObjectives(const CommandNodeBuilder& root) const {
    root.literal("list", [this](const CommandNodeBuilder& list) {
        list.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const std::vector<const Objective*> all = std::as_const(*scoreboard_).allObjectives();
            std::vector<std::string> named;
            for (const Objective* objective : all) {
                named.push_back(objective->name);
            }
            context.success(all.empty() ? Component::translatable("commands.scoreboard.objectives.list.empty")
                                        : Component::translatable("commands.scoreboard.objectives.list.success",
                                                                  {Component::text(std::to_string(all.size())),
                                                                   Component::text(joined(named))}));
            return static_cast<std::int32_t>(all.size());
        });
    });

    root.literal("add", [this](const CommandNodeBuilder& form) {
        const auto add = [this](const CommandContext& context, bool named) -> Parsed<std::int32_t> {
            const auto* name = context.get<std::string>("objective");
            const auto* criteria = context.get<std::string>("criteria");
            if (name == nullptr || criteria == nullptr) {
                return 0;
            }
            if (name->size() > kMaxNameLength) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::text("An objective name may be at most " +
                                                                    std::to_string(kMaxNameLength) + " characters.")}};
            }
            const auto* label = named ? context.get<Component>("displayName") : nullptr;
            Objective objective{.name = *name,
                                .criterion = *criteria,
                                .displayName = label == nullptr ? Component::text(*name) : *label};
            const std::string created = objective.name;
            if (!scoreboard_->add(std::move(objective))) {
                return std::unexpected{CommandSyntaxError{
                    .component = Component::translatable("commands.scoreboard.objectives.add.duplicate")}};
            }
            const Objective& stored = *scoreboard_->get(created);
            broadcast(context, play::SetObjectivePacket{.objectiveName = stored.name,
                                                        .method = play::SetObjectivePacket::kMethodAdd,
                                                        .displayName = stored.displayName,
                                                        .renderType = stored.renderType});
            context.success(Component::translatable("commands.scoreboard.objectives.add.success", {stored.displayName}),
                            true);
            return static_cast<std::int32_t>(scoreboard_->allObjectives().size());
        };
        form.argument("objective", wordArgument(), [add](const CommandNodeBuilder& name) {
            name.argument("criteria", objectiveCriteriaArgument(), [add](const CommandNodeBuilder& criteria) {
                criteria.executes([add](const CommandContext& context) { return add(context, false); });
                criteria.argument("displayName", componentArgument(), [add](const CommandNodeBuilder& label) {
                    label.executes([add](const CommandContext& context) { return add(context, true); });
                });
            });
        });
    });

    root.literal("remove", [this](const CommandNodeBuilder& form) {
        form.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
            named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto objective = objectiveOf(*scoreboard_, context);
                if (!objective.has_value()) {
                    return std::unexpected{objective.error()};
                }
                const std::string name = (*objective)->name;
                const Component label = (*objective)->displayName;
                static_cast<void>(scoreboard_->remove(name));
                broadcast(context, play::SetObjectivePacket{.objectiveName = name,
                                                            .method = play::SetObjectivePacket::kMethodRemove});
                context.success(Component::translatable("commands.scoreboard.objectives.remove.success", {label}),
                                true);
                return static_cast<std::int32_t>(scoreboard_->allObjectives().size());
            });
        });
    });

    root.literal("setdisplay", [this](const CommandNodeBuilder& form) {
        form.argument("slot", scoreboardSlotArgument(), [this](const CommandNodeBuilder& slot) {
            slot.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* where = context.get<play::DisplaySlot>("slot");
                if (where == nullptr) {
                    return 0;
                }
                if (scoreboard_->displayed(*where) == nullptr) {
                    return std::unexpected{CommandSyntaxError{
                        .component = Component::translatable("commands.scoreboard.objectives.display.alreadyEmpty")}};
                }
                scoreboard_->display(*where, std::nullopt);
                broadcast(context, play::SetDisplayObjectivePacket{.slot = *where, .objectiveName = ""});
                context.success(Component::translatable("commands.scoreboard.objectives.display.cleared",
                                                        {Component::text(slotName(*where))}),
                                true);
                return 0;
            });
            slot.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
                named.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                    const auto* where = context.get<play::DisplaySlot>("slot");
                    const auto objective = objectiveOf(*scoreboard_, context);
                    if (where == nullptr) {
                        return 0;
                    }
                    if (!objective.has_value()) {
                        return std::unexpected{objective.error()};
                    }
                    const std::string* current = scoreboard_->displayed(*where);
                    if (current != nullptr && *current == (*objective)->name) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.scoreboard.objectives.display.alreadySet")}};
                    }
                    scoreboard_->display(*where, (*objective)->name);
                    broadcast(context,
                              play::SetDisplayObjectivePacket{.slot = *where, .objectiveName = (*objective)->name});
                    // Whatever was already scored has to be sent again: the client only keeps lines
                    // for an objective it is currently showing.
                    for (const std::string& holder : scoreboard_->holdersIn((*objective)->name)) {
                        const std::optional<std::int32_t> score = scoreboard_->score(holder, (*objective)->name);
                        if (score.has_value()) {
                            broadcast(context, play::SetScorePacket{.owner = holder,
                                                                    .objectiveName = (*objective)->name,
                                                                    .score = *score});
                        }
                    }
                    context.success(
                        Component::translatable("commands.scoreboard.objectives.display.set",
                                                {Component::text(slotName(*where)), (*objective)->displayName}),
                        true);
                    return 0;
                });
            });
        });
    });

    root.literal("modify", [this](const CommandNodeBuilder& form) {
        form.argument("objective", objectiveArgument(), [this](const CommandNodeBuilder& named) {
            named.literal("displayname", [this](const CommandNodeBuilder& label) {
                label.argument("displayName", componentArgument(), [this](const CommandNodeBuilder& value) {
                    value.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto objective = objectiveOf(*scoreboard_, context);
                        if (!objective.has_value()) {
                            return std::unexpected{objective.error()};
                        }
                        const auto* wanted = context.get<Component>("displayName");
                        if (wanted == nullptr) {
                            return 0;
                        }
                        (*objective)->displayName = *wanted;
                        broadcast(context, play::SetObjectivePacket{.objectiveName = (*objective)->name,
                                                                    .method = play::SetObjectivePacket::kMethodChange,
                                                                    .displayName = (*objective)->displayName,
                                                                    .renderType = (*objective)->renderType});
                        context.success(
                            Component::translatable("commands.scoreboard.objectives.modify.displayname",
                                                    {Component::text((*objective)->name), (*objective)->displayName}),
                            true);
                        return 0;
                    });
                });
            });
            named.literal("rendertype", [this](const CommandNodeBuilder& types) {
                for (const play::ObjectiveRenderType type :
                     {play::ObjectiveRenderType::Integer, play::ObjectiveRenderType::Hearts}) {
                    types.literal(renderTypeName(type), [this, type](const CommandNodeBuilder& one) {
                        one.executes([this, type](const CommandContext& context) -> Parsed<std::int32_t> {
                            const auto objective = objectiveOf(*scoreboard_, context);
                            if (!objective.has_value()) {
                                return std::unexpected{objective.error()};
                            }
                            (*objective)->renderType = type;
                            broadcast(context,
                                      play::SetObjectivePacket{.objectiveName = (*objective)->name,
                                                               .method = play::SetObjectivePacket::kMethodChange,
                                                               .displayName = (*objective)->displayName,
                                                               .renderType = type});
                            context.success(Component::translatable("commands.scoreboard.objectives.modify.rendertype",
                                                                    {(*objective)->displayName}),
                                            true);
                            return 0;
                        });
                    });
                }
            });
            named.literal("displayautoupdate", [this](const CommandNodeBuilder& form) {
                form.argument("value", boolArgument(), [this](const CommandNodeBuilder& value) {
                    value.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                        const auto objective = objectiveOf(*scoreboard_, context);
                        if (!objective.has_value()) {
                            return std::unexpected{objective.error()};
                        }
                        const auto* wanted = context.get<bool>("value");
                        (*objective)->displayAutoUpdate = wanted != nullptr && *wanted;
                        context.success(
                            Component::translatable("commands.scoreboard.objectives.modify.displayautoupdate",
                                                    {(*objective)->displayName}),
                            true);
                        return 0;
                    });
                });
            });
            named.literal("numberformat", [this](const CommandNodeBuilder& form) {
                const auto set = [this](const CommandContext& context,
                                        std::optional<play::NumberFormat> format) -> Parsed<std::int32_t> {
                    const auto objective = objectiveOf(*scoreboard_, context);
                    if (!objective.has_value()) {
                        return std::unexpected{objective.error()};
                    }
                    (*objective)->numberFormat = format;
                    resend(context, **objective);
                    context.success(Component::translatable(
                                        format.has_value() ? "commands.scoreboard.objectives.modify.numberFormat"
                                                           : "commands.scoreboard.objectives.modify.reset.numberFormat",
                                        {(*objective)->displayName}),
                                    true);
                    return 0;
                };
                form.executes([set](const CommandContext& context) { return set(context, std::nullopt); });
                numberFormats(form, set);
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
