#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `SnowyBlock`.
namespace snowyBlocks {

/// parity: `BlockTags.SNOW`, which is what `isSnowySetting` asks.
[[nodiscard]] bool isSnow(std::int32_t stateId);

[[nodiscard]] bool wearsSnow(std::int32_t stateId);

[[nodiscard]] std::optional<std::int32_t> updated(const WorldState& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

} // namespace snowyBlocks

} // namespace basalt::server::runtime::world
