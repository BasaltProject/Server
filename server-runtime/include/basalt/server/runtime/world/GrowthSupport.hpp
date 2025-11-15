#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::growthSupport {

/// Primitives shared by more than one block-family growth object.
///
/// Kept apart from any single family so none of them has to depend on another for a horizontal
/// offset or a light threshold.
inline constexpr std::int32_t kGrowthLight = 9;

inline constexpr std::array<std::string_view, 4> kHorizontalNames{"north", "south", "west", "east"};

inline constexpr std::array<std::string_view, 6> kFaceNames{"down", "up", "north", "south", "west", "east"};

/// Writes the block and records the change, or does neither when the position is not loaded.
void set(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         std::vector<BlockChange>& changes);

[[nodiscard]] std::int32_t offsetX(std::string_view facing);

[[nodiscard]] std::int32_t offsetY(std::string_view facing);

[[nodiscard]] std::int32_t offsetZ(std::string_view facing);

/// Advances an age property by one, shared by every simple age-based crop.
void grow(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, std::int32_t maxAge,
          std::vector<BlockChange>& changes);

/// The `age` of a block state, or nothing when it has none or it is not a number.
[[nodiscard]] std::optional<std::int32_t> ageOf(std::int32_t stateId);

} // namespace basalt::server::runtime::world::growthSupport
