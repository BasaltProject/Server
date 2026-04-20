#pragma once

#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/wire/text/Component.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::scoreboard {

[[nodiscard]] wire::text::Component nameTagFor(const Teams& teams, std::string_view member, std::string_view viewer);

[[nodiscard]] std::optional<wire::text::TextColor> textColorOf(wire::protocol::play::TeamColor colour);

} // namespace basalt::server::runtime::scoreboard
