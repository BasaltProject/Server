#pragma once

#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace basalt::server::runtime::command::commands::scoreboardCommand {

using scoreboard::Objective;

[[nodiscard]] std::string joined(const std::vector<std::string>& names);

template <typename T>
void broadcast(const CommandContext& context, const T& packet) {
    namespace play = wire::protocol::play;
    for (const player::PlayerSession* player : context.environment().players().snapshot()) {
        const player::PlayerConnection& connection = player->connection();
        if constexpr (std::is_same_v<T, play::SetObjectivePacket>) {
            if (connection.sendObjective) {
                connection.sendObjective(packet.objectiveName, packet.method, packet.displayName);
            }
        } else if constexpr (std::is_same_v<T, play::SetDisplayObjectivePacket>) {
            if (connection.sendDisplayObjective) {
                connection.sendDisplayObjective(packet.slot, packet.objectiveName);
            }
        } else if constexpr (std::is_same_v<T, play::SetScorePacket>) {
            if (connection.sendScore) {
                connection.sendScore(packet.owner, packet.objectiveName, packet.score);
            }
        } else {
            static_assert(std::is_same_v<T, play::ResetScorePacket>);
            if (connection.sendScoreRemoved) {
                connection.sendScoreRemoved(packet.owner, packet.objectiveName);
            }
        }
    }
}

[[nodiscard]] std::expected<Objective*, CommandSyntaxError>
objectiveOf(scoreboard::Scoreboard& scoreboard, const CommandContext& context, std::string_view name = "objective");

void sendScore(const scoreboard::Scoreboard& scoreboard, const CommandContext& context, const std::string& holder,
               const std::string& objectiveName, std::int32_t value);

void resend(const CommandContext& context, const Objective& objective);

void numberFormats(const CommandNodeBuilder& node,
                   const std::function<Parsed<std::int32_t>(const CommandContext&,
                                                            std::optional<wire::protocol::play::NumberFormat>)>& run);

[[nodiscard]] const std::vector<std::string>& holdersOf(const CommandContext& context, std::string_view name);

} // namespace basalt::server::runtime::command::commands::scoreboardCommand
