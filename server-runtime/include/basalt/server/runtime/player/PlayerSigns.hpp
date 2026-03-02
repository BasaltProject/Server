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

/// What a plugin gets a say in: the lines are its to edit, and false refuses the whole write.
using SignChangeAllowed = std::function<bool(PlayerSession& who, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::vector<std::string>& lines)>;

/// Writes a sign's text and tells everyone who can see it.
///
/// Shared because both doors write the same block: the Java session gets four lines from its client
/// and the Bedrock one gets a single string it has to cut up first.
bool writeSignFor(const BreakContext& context, world::LoadedWorld& world, PlayerSession& session, std::int32_t x,
                  std::int32_t y, std::int32_t z, bool front, std::vector<std::string> lines,
                  const SignChangeAllowed& allowed = {});

/// The four lines out of the one string that edition holds a sign in.
///
/// It wraps its own text to the width of the sign as the player types; this server has no font to
/// measure with, so only the newlines are cut on and anything past the fourth line is dropped.
[[nodiscard]] std::vector<std::string> signLinesOf(std::string_view text);

} // namespace basalt::server::runtime::player
