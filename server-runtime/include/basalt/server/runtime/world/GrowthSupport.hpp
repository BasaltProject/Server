#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::growthSupport {

/// Kept apart from any one family so none depends on another for a horizontal offset or light threshold.
inline constexpr std::int32_t kGrowthLight = 9;

/// parity: `Direction.Plane.HORIZONTAL`.
inline constexpr std::array<std::string_view, 4> kHorizontalNames{"north", "east", "south", "west"};

inline constexpr std::array<std::string_view, 6> kFaceNames{"down", "up", "north", "south", "west", "east"};

void set(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         std::vector<BlockChange>& changes);

[[nodiscard]] std::int32_t offsetX(std::string_view facing);

[[nodiscard]] std::int32_t offsetY(std::string_view facing);

[[nodiscard]] std::int32_t offsetZ(std::string_view facing);

void grow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t maxAge,
          std::vector<BlockChange>& changes);

[[nodiscard]] std::optional<std::int32_t> ageOf(std::int32_t stateId);

} // namespace basalt::server::runtime::world::growthSupport
