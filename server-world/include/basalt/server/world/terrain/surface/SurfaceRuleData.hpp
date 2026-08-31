#pragma once

#include <basalt/server/world/terrain/surface/SurfaceRules.hpp>

namespace basalt::server::world::terrain::surface::surfaceRuleData {

/// parity: `SurfaceRuleData.overworld`, always the real `overworldLike(true, false, true)`.
///
/// Built once and cached rather than per chunk: no node holds mutable state, every one of them
/// reads from the environment and context handed to it at evaluation time, so a single instance is
/// safe to share across chunk-generation threads.
[[nodiscard]] const RuleSource& overworld();

/// parity: `SurfaceRuleData.nether`.
[[nodiscard]] const RuleSource& nether();

/// parity: `SurfaceRuleData.overworldLike`.
[[nodiscard]] RuleSource overworldLike(bool doPreliminarySurfaceCheck, bool bedrockRoof, bool bedrockFloor);

} // namespace basalt::server::world::terrain::surface::surfaceRuleData
