#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRules.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::world::terrain::surface {

namespace surfaceBuilder {

/// parity: `SurfaceSystem.topMaterial`, which carvers use to repair dirt just below a grass or
/// mycelium surface they opened. Vanilla evaluates the normal rule with both stone depths fixed to
/// one and only exposes the carved fluid as the synthetic water height.
[[nodiscard]] std::optional<std::int32_t> topMaterial(const SurfaceRuleEnvironment& environment,
                                                      const RuleSource& ruleSource, std::int32_t x, std::int32_t y,
                                                      std::int32_t z, bool underFluid,
                                                      std::int32_t generationMinY = kOverworldHeight.minY(),
                                                      std::int32_t generationHeight = kOverworldHeight.height());

/// parity: `SurfaceSystem.buildSurface`, the per-column top-down traversal plus the eroded-badlands
/// and frozen-ocean extensions.
///
/// Tracks `stoneAboveDepth` the way vanilla does and a lookahead-cached `stoneBelowDepth`, replacing
/// every block still exactly `defaultBlockStateId` with whatever the rule tree returns.
void buildSurface(Chunk& chunk, const SurfaceRuleEnvironment& environment, const RuleSource& ruleSource,
                  std::optional<std::int32_t> defaultBlockStateId = std::nullopt,
                  std::optional<std::int32_t> generationMinY = std::nullopt,
                  std::optional<std::int32_t> generationHeight = std::nullopt);

} // namespace surfaceBuilder

} // namespace basalt::server::world::terrain::surface
