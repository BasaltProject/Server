#pragma once

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {

/// How many people have a given container open. parity: `ContainerOpenersCounter`, which raises a
/// chest lid and gives a trapped chest its redstone. Never written to disk, so a container reopened
/// from a saved world is closed.
///
/// Opening one is rare and asking is not, so a count taken while nothing is open anywhere costs one
/// atomic read and no lock: that is the redstone path.
namespace containerOpeners {

/// One more looking. Answers the new count, which is what the block event carries.
std::int32_t open(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

/// One fewer. Answers the new count; at zero the entry goes rather than sitting at zero.
std::int32_t close(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::int32_t count(std::string_view world, std::int32_t x, std::int32_t y, std::int32_t z);

/// Everyone left, so nothing is open anywhere. For a world unloading or the server stopping.
void clear();

} // namespace containerOpeners

} // namespace basalt::server::runtime::world
