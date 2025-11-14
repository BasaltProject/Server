#include <basalt/server/runtime/scoreboard/Teams.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::scoreboard {

using wire::protocol::play::SetPlayerTeamPacket;
using wire::text::Component;

Team Team::named(std::string name) {
    Team team;
    team.displayName = Component::text(name);
    team.name = std::move(name);
    return team;
}

bool Team::hasMember(std::string_view member) const { return std::ranges::find(members, member) != members.end(); }

bool Team::addMember(std::string member) {
    if (hasMember(member)) {
        return false;
    }
    members.push_back(std::move(member));
    return true;
}

bool Team::removeMember(std::string_view member) {
    const auto at = std::ranges::find(members, member);
    if (at == members.end()) {
        return false;
    }
    members.erase(at);
    return true;
}

SetPlayerTeamPacket::Parameters Team::parameters() const {
    return SetPlayerTeamPacket::Parameters{.displayName = displayName,
                                           .prefix = prefix,
                                           .suffix = suffix,
                                           .nameTagVisibility = nameTagVisibility,
                                           .collisionRule = collisionRule,
                                           .color = color,
                                           .friendlyFire = friendlyFire,
                                           .seeFriendlyInvisibles = seeFriendlyInvisibles};
}

std::vector<Team*> Teams::all() {
    std::vector<Team*> every;
    every.reserve(teams_.size());
    for (const std::unique_ptr<Team>& team : teams_) {
        every.push_back(team.get());
    }
    return every;
}

std::vector<const Team*> Teams::all() const {
    std::vector<const Team*> every;
    every.reserve(teams_.size());
    for (const std::unique_ptr<Team>& team : teams_) {
        every.push_back(team.get());
    }
    return every;
}

const Team* Teams::get(std::string_view name) const {
    for (const std::unique_ptr<Team>& team : teams_) {
        if (team->name == name) {
            return team.get();
        }
    }
    return nullptr;
}

Team* Teams::get(std::string_view name) {
    for (const std::unique_ptr<Team>& team : teams_) {
        if (team->name == name) {
            return team.get();
        }
    }
    return nullptr;
}

Team* Teams::add(std::string name) {
    if (get(name) != nullptr) {
        return nullptr;
    }
    teams_.push_back(std::make_unique<Team>(Team::named(std::move(name))));
    return teams_.back().get();
}

std::unique_ptr<Team> Teams::remove(std::string_view name) {
    const auto at =
        std::ranges::find_if(teams_, [name](const std::unique_ptr<Team>& team) { return team->name == name; });
    if (at == teams_.end()) {
        return nullptr;
    }
    std::unique_ptr<Team> removed = std::move(*at);
    teams_.erase(at);
    return removed;
}

const Team* Teams::teamOf(std::string_view member) const {
    for (const std::unique_ptr<Team>& team : teams_) {
        if (team->hasMember(member)) {
            return team.get();
        }
    }
    return nullptr;
}

Team* Teams::teamOf(std::string_view member) {
    for (const std::unique_ptr<Team>& team : teams_) {
        if (team->hasMember(member)) {
            return team.get();
        }
    }
    return nullptr;
}

Team* Teams::join(Team& team, std::string member) {
    Team* previous = teamOf(member);
    if (previous != nullptr) {
        previous->removeMember(member);
    }
    team.addMember(std::move(member));
    return previous;
}

Team* Teams::leave(std::string_view member) {
    Team* previous = teamOf(member);
    if (previous == nullptr) {
        return nullptr;
    }
    previous->removeMember(member);
    return previous;
}

} // namespace basalt::server::runtime::scoreboard
