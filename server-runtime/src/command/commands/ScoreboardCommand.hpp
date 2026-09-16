#pragma once

#include <basalt/server/runtime/command/commands/ScoreboardCommand.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

/// Shared between the two halves of `/scoreboard`, which live in a file each: both send the same
/// four packets and look objectives up the same way.
namespace basalt::server::runtime::command::commands::scoreboardCommand {

using scoreboard::Objective;

[[nodiscard]] std::string joined(const std::vector<std::string>& names);

/// Every player sees a scoreboard change, since the scoreboard is server-wide.
///
/// The four go out through the connection rather than as packets, because the other edition draws a
/// board out of different ones: it has no packet for an objective nobody is shown, and its lines are
/// numbered rather than named.
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

/// The objective an argument named, or the refusal for one nobody made.
[[nodiscard]] std::expected<Objective*, CommandSyntaxError>
objectiveOf(scoreboard::Scoreboard& scoreboard, const CommandContext& context, std::string_view name = "objective");

/// A score only reaches clients while its objective is on screen somewhere.
void sendScore(const scoreboard::Scoreboard& scoreboard, const CommandContext& context, const std::string& holder,
               const std::string& objectiveName, std::int32_t value);

/// Re-sends the objective so a client redraws it with whatever changed.
void resend(const CommandContext& context, const Objective& objective);

/// The three shapes a number format takes, declared once for both places that accept one.
void numberFormats(const CommandNodeBuilder& node,
                   const std::function<Parsed<std::int32_t>(const CommandContext&,
                                                            std::optional<wire::protocol::play::NumberFormat>)>& run);

/// The holders an argument named, which is a selector, a `*` or one invented name.
[[nodiscard]] const std::vector<std::string>& holdersOf(const CommandContext& context, std::string_view name);

} // namespace basalt::server::runtime::command::commands::scoreboardCommand
