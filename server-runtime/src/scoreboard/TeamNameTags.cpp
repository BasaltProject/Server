#include <basalt/server/runtime/scoreboard/TeamNameTags.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <array>

namespace basalt::server::runtime::scoreboard {
namespace {

/// parity: `ChatFormatting`'s own order, which is what `TeamColor` counts in: the sixteen colours
/// come first and in the order their codes run.
[[nodiscard]] std::string legacyColour(const wire::protocol::play::TeamColor colour) {
    constexpr std::array<char, 16> kCodes{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const auto at = static_cast<std::size_t>(colour);
    return at < kCodes.size() ? std::string{"§"} + kCodes[at] : std::string{};
}

} // namespace

std::string nameTagFor(const Teams& teams, const std::string_view member, const std::string_view viewer) {
    using wire::protocol::play::NameTagVisibility;

    const Team* team = teams.teamOf(member);
    if (team == nullptr) {
        return std::string{member};
    }
    // parity: `Team.Visibility`, which is about who is looking rather than about the name itself.
    const bool together = team->hasMember(viewer);
    const bool hidden = team->nameTagVisibility == NameTagVisibility::Never ||
                        (team->nameTagVisibility == NameTagVisibility::HideForOtherTeams && !together) ||
                        (team->nameTagVisibility == NameTagVisibility::HideForOwnTeam && together);
    if (hidden) {
        return {};
    }
    const std::string prefix = wire::text::toLegacyText(team->prefix);
    const std::string suffix = wire::text::toLegacyText(team->suffix);
    const std::string colour = team->color.has_value() ? legacyColour(*team->color) : std::string{};
    return prefix + colour + std::string{member} + suffix;
}

} // namespace basalt::server::runtime::scoreboard
