#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// Whether a block would stay where it is. parity: each block's own `canSurvive`. Most of it is the
/// generated support table, built by asking the jar one neighbour at a time; the few rules a single
/// neighbour cannot express are written out here, each saying why.
///
/// Kept apart from the neighbour cascade that uses it: this half is a question about one position.
namespace blockSurvival {

/// parity: `CropBlock.hasSufficientLight`, which is eight and not the nine growing needs.
inline constexpr std::int32_t kCropSurvivalLight = 8;

/// Whether the scaffolding at this position has lost its footing.
///
/// parity: `ScaffoldingBlock.canSurvive`, which is `getDistance(level, pos) < 7`: a number worked
/// out from the whole row, not something a single neighbour can answer. A callback because that
/// walk lives with the scheduled blocks; without one, scaffolding is left standing.
using ScaffoldingHasFallen =
    std::function<bool(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Whether the block at these coordinates would stay where it is.
[[nodiscard]] bool isSupported(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::optional<std::int32_t> stateId = std::nullopt,
                               const ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

} // namespace blockSurvival

} // namespace basalt::server::runtime::world
