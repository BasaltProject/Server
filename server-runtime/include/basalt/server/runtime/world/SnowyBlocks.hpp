#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The white top a block of grass wears while there is snow on it.
///
/// parity: `SnowyBlock`, whose `snowy` property is read off the block above, both when it is placed
/// and every time that neighbour changes. Without the second half, breaking a snow layer leaves the
/// grass white for ever, which is what a real client showed.
namespace snowyBlocks {

/// parity: `BlockTags.SNOW`, which is what `isSnowySetting` asks.
[[nodiscard]] bool isSnow(std::int32_t stateId);

/// Whether this block wears snow at all: grass, podzol and mycelium do.
[[nodiscard]] bool wearsSnow(std::int32_t stateId);

/// The block at these coordinates with its `snowy` set from whatever is above it now, or nothing
/// when it does not wear snow or already looks right.
[[nodiscard]] std::optional<std::int32_t> updated(const WorldState& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

} // namespace snowyBlocks

} // namespace basalt::server::runtime::world
