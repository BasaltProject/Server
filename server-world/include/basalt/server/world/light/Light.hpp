#pragma once

#include <basalt/server/world/light/LightWindow.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::world::light {

using SectionLight = std::vector<std::int32_t>;

/// parity: `SkyLightEngine`.
namespace skyLight {

inline constexpr std::int32_t kMax = 15;

/// parity: `BlockBehaviour.getLightDampening`.
[[nodiscard]] std::int32_t opacity(std::int32_t stateId);

[[nodiscard]] std::span<const std::int32_t> opacityTable();

[[nodiscard]] std::vector<SectionLight> compute(const LightWindow& window);

} // namespace skyLight

/// parity: `BlockLightEngine`.
namespace blockLight {

/// parity: `BlockBehaviour.Properties.lightLevel` of the blocks that have one.
[[nodiscard]] std::int32_t emission(std::int32_t stateId);

[[nodiscard]] std::optional<std::vector<SectionLight>> compute(const LightWindow& window);

} // namespace blockLight

} // namespace basalt::server::world::light
