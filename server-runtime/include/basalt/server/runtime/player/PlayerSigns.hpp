#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

using SignChangeAllowed = std::function<bool(PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::vector<std::string>& lines)>;

bool writeSignFor(const BreakContext& context, world::LoadedWorld& world, PlayerSession& session, std::int32_t x,
                  std::int32_t y, std::int32_t z, bool front, std::vector<std::string> lines,
                  const SignChangeAllowed& allowed = {});

[[nodiscard]] std::vector<std::string> signLinesOf(std::string_view text);

} // namespace basalt::server::runtime::player
