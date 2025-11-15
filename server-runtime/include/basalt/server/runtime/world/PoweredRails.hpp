#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// The rails that carry their own power along the line.
///
/// parity: `PoweredRailBlock.updateState`. A powered or activator rail is on when something powers
/// it directly, or when a rail of its own kind up to eight along is on and *that* one is powered
/// directly. That is what makes one lever light a whole run of them, and why the ninth is dark.
namespace poweredRails {

/// parity: `findPoweredRailSignal`'s own eight, counted from the rail being asked about.
inline constexpr std::int32_t kSearchLimit = 8;

/// Answers the change if the rail turned on or off, which is what the caller sends out.
[[nodiscard]] std::optional<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                                std::int32_t y, std::int32_t z);

} // namespace poweredRails

} // namespace basalt::server::runtime::world
