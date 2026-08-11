#pragma once

#include <basalt/wire/text/Component.hpp>

#include <string_view>

namespace basalt::server::runtime::world {

/// What a player types into a sign or an anvil: `&` followed by a colour or format letter, or `&#rrggbb`.
namespace colourCodes {

[[nodiscard]] bool hasCodes(std::string_view text);

[[nodiscard]] wire::text::Component ofAmpersands(std::string_view text);

} // namespace colourCodes

} // namespace basalt::server::runtime::world
