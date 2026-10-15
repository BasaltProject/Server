#pragma once

#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::scoreboard {

struct Team {
    std::string name;
    wire::text::Component displayName;
    wire::text::Component prefix = wire::text::Component::empty();
    wire::text::Component suffix = wire::text::Component::empty();
    std::optional<wire::protocol::play::TeamColor> color;
    bool friendlyFire = true;
    bool seeFriendlyInvisibles = true;
    wire::protocol::play::NameTagVisibility nameTagVisibility = wire::protocol::play::NameTagVisibility::Always;

    wire::protocol::play::NameTagVisibility deathMessageVisibility = wire::protocol::play::NameTagVisibility::Always;
    wire::protocol::play::CollisionRule collisionRule = wire::protocol::play::CollisionRule::Always;

    std::vector<std::string> members;

    [[nodiscard]] static Team named(std::string name);

    [[nodiscard]] bool hasMember(std::string_view member) const;

    /// Answers whether the member was not already there, as vanilla's `Set.add` does.
    bool addMember(std::string member);

    bool removeMember(std::string_view member);

    [[nodiscard]] wire::protocol::play::SetPlayerTeamPacket::Parameters parameters() const;
};

class Teams {
public:
    [[nodiscard]] std::vector<Team*> all();
    [[nodiscard]] std::vector<const Team*> all() const;

    [[nodiscard]] Team* get(std::string_view name);
    [[nodiscard]] const Team* get(std::string_view name) const;

    Team* add(std::string name);

    std::unique_ptr<Team> remove(std::string_view name);

    [[nodiscard]] Team* teamOf(std::string_view member);
    [[nodiscard]] const Team* teamOf(std::string_view member) const;

    Team* join(Team& team, std::string member);

    Team* leave(std::string_view member);

private:
    std::vector<std::unique_ptr<Team>> teams_;
};

} // namespace basalt::server::runtime::scoreboard
