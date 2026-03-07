#pragma once

#include <basalt/server/runtime/world/Books.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Writes what a player typed onto the book in `slot` (hotbar 0 to 8, off hand 40), and signs it
/// when they gave it a title. parity: `handleEditBook`.
[[nodiscard]] bool editBookFor(PlayerSession& session, std::int32_t slot, const std::vector<std::string>& pages,
                               const std::optional<std::string>& title);

/// The same book, a page at a time: the shape the other edition sends. `slot` is numbered the same
/// way.
[[nodiscard]] bool editBookPageFor(PlayerSession& session, std::int32_t slot, world::books::PageEdit what,
                                   std::int32_t page, std::int32_t other, const std::string& text);

} // namespace basalt::server::runtime::player
