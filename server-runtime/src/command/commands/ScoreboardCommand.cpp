#include "ScoreboardCommand.hpp"

#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/MessageArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>

#include <cstdint>

namespace basalt::server::runtime::command::commands {
namespace scoreboardCommand {
namespace {

namespace play = wire::protocol::play;

using wire::text::Component;

} // namespace

std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const std::string& name : names) {
        text += text.empty() ? name : ", " + name;
    }
    return text;
}

std::expected<Objective*, CommandSyntaxError> objectiveOf(scoreboard::Scoreboard& scoreboard,
                                                          const CommandContext& context, std::string_view name) {
    const auto* asked = context.get<std::string>(name);
    Objective* objective = asked == nullptr ? nullptr : scoreboard.get(*asked);
    if (objective == nullptr) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("arguments.objective.notFound",
                                                 {Component::text(asked == nullptr ? std::string{} : *asked)})}};
    }
    return objective;
}

void sendScore(const scoreboard::Scoreboard& scoreboard, const CommandContext& context, const std::string& holder,
               const std::string& objectiveName, std::int32_t value) {
    for (const play::DisplaySlot slot :
         {play::DisplaySlot::List, play::DisplaySlot::Sidebar, play::DisplaySlot::BelowName}) {
        const std::string* shown = scoreboard.displayed(slot);
        if (shown != nullptr && *shown == objectiveName) {
            broadcast(context, play::SetScorePacket{.owner = holder, .objectiveName = objectiveName, .score = value});
            return;
        }
    }
}

void resend(const CommandContext& context, const Objective& objective) {
    broadcast(context, play::SetObjectivePacket{.objectiveName = objective.name,
                                                .method = play::SetObjectivePacket::kMethodChange,
                                                .displayName = objective.displayName,
                                                .renderType = objective.renderType,
                                                .numberFormat = objective.numberFormat});
}

void numberFormats(
    const CommandNodeBuilder& node,
    const std::function<Parsed<std::int32_t>(const CommandContext&, std::optional<play::NumberFormat>)>& run) {
    node.literal("blank", [run](const CommandNodeBuilder& blank) {
        blank.executes([run](const CommandContext& context) {
            return run(context, play::NumberFormat{play::NumberFormat::Blank{}});
        });
    });
    node.literal("fixed", [run](const CommandNodeBuilder& fixed) {
        fixed.argument("contents", componentArgument(), [run](const CommandNodeBuilder& contents) {
            contents.executes([run](const CommandContext& context) {
                const auto* value = command::messageArguments::resolvedComponent(context, "contents");
                return run(context, play::NumberFormat{
                                        play::NumberFormat::Fixed{.value = value == nullptr ? Component{} : *value}});
            });
        });
    });
    node.literal("styled", [run](const CommandNodeBuilder& styled) {
        styled.argument("style", styleArgument(), [run](const CommandNodeBuilder& style) {
            style.executes([run](const CommandContext& context) {
                const auto* value = context.get<wire::nbt::NbtCompound>("style");
                return run(context, play::NumberFormat{play::NumberFormat::Styled{
                                        .style = value == nullptr ? wire::nbt::NbtCompound{} : *value}});
            });
        });
    });
}

const std::vector<std::string>& holdersOf(const CommandContext& context, std::string_view name) {
    static const std::vector<std::string> none;
    const auto* found = context.get<std::vector<std::string>>(name);
    return found == nullptr ? none : *found;
}

} // namespace scoreboardCommand

ScoreboardCommand::ScoreboardCommand(scoreboard::Scoreboard& scoreboard)
    : Command("scoreboard", permissionLevel::kGamemasters), scoreboard_(&scoreboard) {}

void ScoreboardCommand::build(const CommandNodeBuilder& root) const {
    root.literal("objectives", [this](const CommandNodeBuilder& objectives) { buildObjectives(objectives); });
    root.literal("players", [this](const CommandNodeBuilder& players) { buildPlayers(players); });
}

} // namespace basalt::server::runtime::command::commands
