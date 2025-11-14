#pragma once

#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::scoreboard {

/// One team, with everything `/team modify` can change about it. parity: `PlayerTeam`, whose own
/// defaults these are.
struct Team {
    std::string name;
    wire::text::Component displayName;
    wire::text::Component prefix = wire::text::Component::empty();
    wire::text::Component suffix = wire::text::Component::empty();
    std::optional<wire::protocol::play::TeamColor> color;
    bool friendlyFire = true;
    bool seeFriendlyInvisibles = true;
    wire::protocol::play::NameTagVisibility nameTagVisibility = wire::protocol::play::NameTagVisibility::Always;

    /// parity: `deathMessageVisibility`, which decides who reads this team's death messages.
    wire::protocol::play::NameTagVisibility deathMessageVisibility = wire::protocol::play::NameTagVisibility::Always;
    wire::protocol::play::CollisionRule collisionRule = wire::protocol::play::CollisionRule::Always;

    /// Member names, which for a player is their name and for anything else its UUID, as vanilla
    /// does. Kept in the order they joined: vanilla's own `Set` is unordered, and a listing that at
    /// least does not reshuffle itself between calls is the friendlier half of that freedom.
    std::vector<std::string> members;

    /// A fresh team, whose display name is its own name until `/team modify` says otherwise.
    [[nodiscard]] static Team named(std::string name);

    [[nodiscard]] bool hasMember(std::string_view member) const;

    /// Answers whether the member was not already there, as vanilla's `Set.add` does.
    bool addMember(std::string member);

    bool removeMember(std::string_view member);

    [[nodiscard]] wire::protocol::play::SetPlayerTeamPacket::Parameters parameters() const;
};

/// Every team on the server, and who is in them. A member belongs to at most one, as in vanilla.
class Teams {
public:
    [[nodiscard]] std::vector<Team*> all();
    [[nodiscard]] std::vector<const Team*> all() const;

    [[nodiscard]] Team* get(std::string_view name);
    [[nodiscard]] const Team* get(std::string_view name) const;

    /// Answers the new team, or null when one already goes by that name.
    Team* add(std::string name);

    /// Hands the team back rather than dropping it, because whoever removed it still has to tell
    /// the clients which one went.
    std::unique_ptr<Team> remove(std::string_view name);

    [[nodiscard]] Team* teamOf(std::string_view member);
    [[nodiscard]] const Team* teamOf(std::string_view member) const;

    /// Puts `member` in `team`, taking them out of whatever team they were in. Returns the old one.
    Team* join(Team& team, std::string member);

    Team* leave(std::string_view member);

private:
    std::vector<std::unique_ptr<Team>> teams_;
};

} // namespace basalt::server::runtime::scoreboard
