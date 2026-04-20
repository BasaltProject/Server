#include <basalt/server/runtime/scoreboard/TeamNameTags.hpp>
#include <basalt/wire/text/TextColor.hpp>

#include <array>
#include <optional>
#include <string>

namespace basalt::server::runtime::scoreboard {

using wire::text::TextColor;

/// parity: `TeamColor` counts in `ChatFormatting`'s order, the sixteen colours first.
std::optional<TextColor> textColorOf(const wire::protocol::play::TeamColor colour) {
    constexpr std::array<TextColor, 16> kColours{
        wire::text::kBlack,    wire::text::kDarkBlue,    wire::text::kDarkGreen, wire::text::kDarkAqua,
        wire::text::kDarkRed,  wire::text::kDarkPurple,  wire::text::kGold,      wire::text::kGray,
        wire::text::kDarkGray, wire::text::kBlue,        wire::text::kGreen,     wire::text::kAqua,
        wire::text::kRed,      wire::text::kLightPurple, wire::text::kYellow,    wire::text::kWhite};
    const auto at = static_cast<std::size_t>(colour);
    return at < kColours.size() ? std::optional<TextColor>{kColours[at]} : std::nullopt;
}

wire::text::Component nameTagFor(const Teams& teams, const std::string_view member, const std::string_view viewer) {
    using wire::protocol::play::NameTagVisibility;
    using wire::text::Component;

    const Team* team = teams.teamOf(member);
    if (team == nullptr) {
        return Component::text(std::string{member});
    }
    // parity: `Team.Visibility`.
    const bool together = team->hasMember(viewer);
    const bool hidden = team->nameTagVisibility == NameTagVisibility::Never ||
                        (team->nameTagVisibility == NameTagVisibility::HideForOtherTeams && !together) ||
                        (team->nameTagVisibility == NameTagVisibility::HideForOwnTeam && together);
    if (hidden) {
        return Component::empty();
    }
    Component name = Component::text(std::string{member});
    if (team->color.has_value()) {
        name = name.color(textColorOf(*team->color));
    }
    return Component::empty().append(team->prefix).append(name).append(team->suffix);
}

} // namespace basalt::server::runtime::scoreboard
