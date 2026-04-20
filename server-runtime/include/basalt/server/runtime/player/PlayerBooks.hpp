#pragma once

#include <basalt/server/runtime/world/Books.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerSession;

[[nodiscard]] bool editBookFor(PlayerSession& session, std::int32_t slot, const std::vector<std::string>& pages,
                               const std::optional<std::string>& title);

[[nodiscard]] bool editBookPageFor(PlayerSession& session, std::int32_t slot, world::books::PageEdit what,
                                   std::int32_t page, std::int32_t other, const std::string& text);

} // namespace basalt::server::runtime::player
