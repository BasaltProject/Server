#pragma once

#include <basalt/server/runtime/scoreboard/Teams.hpp>

#include <string>
#include <string_view>

namespace basalt::server::runtime::scoreboard {

/// The name one player draws over another's head once their team has had its say, or nothing at
/// all where the team hides it from that viewer.
///
/// parity: the other door's client builds this itself out of the team packet, prefix then the name
/// in the team's colour then suffix, and hides it by `Team.Visibility`. The edition that has no
/// teams is told the finished line, so the two show the same thing.
[[nodiscard]] std::string nameTagFor(const Teams& teams, std::string_view member, std::string_view viewer);

} // namespace basalt::server::runtime::scoreboard
