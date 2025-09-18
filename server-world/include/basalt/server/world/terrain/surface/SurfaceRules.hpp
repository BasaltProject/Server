#pragma once

#include <basalt/server/world/terrain/surface/SurfaceRuleContext.hpp>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::surface {

/// parity: `SurfaceRules.Condition`.
using Condition = std::function<bool()>;

/// parity: `SurfaceRules.ConditionSource`.
using ConditionSource = std::function<Condition(SurfaceRuleContext&)>;

/// parity: `SurfaceRules.SurfaceRule`. An empty result means "no match, try the next rule".
using SurfaceRule = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t, std::int32_t)>;

/// parity: `SurfaceRules.RuleSource`.
using RuleSource = std::function<SurfaceRule(SurfaceRuleContext&)>;

/// parity: `net.minecraft.world.level.levelgen.SurfaceRules`, its factory methods and node types.
namespace surfaceRules {

[[nodiscard]] ConditionSource stoneDepthCheck(std::int32_t offset, bool addSurfaceDepth,
                                              std::int32_t secondaryDepthRange, bool ceiling);

[[nodiscard]] const ConditionSource& onFloor();
[[nodiscard]] const ConditionSource& underFloor();
[[nodiscard]] const ConditionSource& deepUnderFloor();
[[nodiscard]] const ConditionSource& veryDeepUnderFloor();
[[nodiscard]] const ConditionSource& onCeiling();
[[nodiscard]] const ConditionSource& underCeiling();

[[nodiscard]] ConditionSource notCondition(ConditionSource target);

[[nodiscard]] ConditionSource yBlockCheck(VerticalAnchor anchor, std::int32_t surfaceDepthMultiplier);
[[nodiscard]] ConditionSource yStartCheck(VerticalAnchor anchor, std::int32_t surfaceDepthMultiplier);

[[nodiscard]] ConditionSource waterBlockCheck(std::int32_t offset, std::int32_t surfaceDepthMultiplier);
[[nodiscard]] ConditionSource waterStartCheck(std::int32_t offset, std::int32_t surfaceDepthMultiplier);

[[nodiscard]] ConditionSource isBiome(std::vector<std::string> target);

[[nodiscard]] ConditionSource noiseCondition2d(std::string noiseId, double minRange,
                                               double maxRange = std::numeric_limits<double>::max());
[[nodiscard]] ConditionSource noiseCondition3d(std::string noiseId, double minRange,
                                               double maxRange = std::numeric_limits<double>::max());

[[nodiscard]] ConditionSource verticalGradient(std::string randomName, VerticalAnchor trueAtAndBelow,
                                               VerticalAnchor falseAtAndAbove);

[[nodiscard]] ConditionSource steep();
[[nodiscard]] ConditionSource hole();
[[nodiscard]] ConditionSource abovePreliminarySurface();

/// parity: `SurfaceRules.temperature`, itself `Biome.coldEnoughToSnow`.
[[nodiscard]] ConditionSource temperature();

[[nodiscard]] RuleSource ifTrue(ConditionSource condition, RuleSource thenRun);
[[nodiscard]] RuleSource sequence(std::vector<RuleSource> rules);
[[nodiscard]] RuleSource state(std::int32_t stateId);
[[nodiscard]] RuleSource bandlands();

} // namespace surfaceRules

} // namespace basalt::server::world::terrain::surface
