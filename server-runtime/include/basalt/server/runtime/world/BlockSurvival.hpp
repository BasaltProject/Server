#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

namespace blockSurvival {

/// parity: `CropBlock.hasSufficientLight`, which is eight and not the nine growing needs.
inline constexpr std::int32_t kCropSurvivalLight = 8;

using ScaffoldingHasFallen =
    std::function<bool(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] bool loosenedBy(std::int32_t stateId, std::optional<server::registry::BlockFace> towards);

[[nodiscard]] bool isSupported(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::optional<std::int32_t> stateId = std::nullopt,
                               const ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

} // namespace blockSurvival

} // namespace basalt::server::runtime::world
