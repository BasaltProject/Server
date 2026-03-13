#pragma once

#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/wire/text/Component.hpp>

#include <string_view>

namespace basalt::server::runtime::scoreboard {

/// The name drawn over `member`'s head for `viewer`, empty where the team hides it. parity: what a
/// Java client builds from the team packet, for the edition that has no teams.
[[nodiscard]] wire::text::Component nameTagFor(const Teams& teams, std::string_view member, std::string_view viewer);

} // namespace basalt::server::runtime::scoreboard
