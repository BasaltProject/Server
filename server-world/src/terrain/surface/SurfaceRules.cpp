#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRules.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace basalt::server::world::terrain::surface {
namespace mth = wire::common::mth;

ConditionSource surfaceRules::stoneDepthCheck(std::int32_t offset, bool addSurfaceDepth,
                                              std::int32_t secondaryDepthRange, bool ceiling) {
    return [offset, addSurfaceDepth, secondaryDepthRange, ceiling](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [offset, addSurfaceDepth, secondaryDepthRange, ceiling, bound] {
            const std::int32_t stoneDepth = ceiling ? bound->stoneDepthBelow() : bound->stoneDepthAbove();
            const std::int32_t surfaceDepth = addSurfaceDepth ? bound->surfaceDepth() : 0;
            const std::int32_t secondarySurfaceDepth =
                secondaryDepthRange == 0
                    ? 0
                    : static_cast<std::int32_t>(mth::map(bound->surfaceSecondary(), -1.0, 1.0, 0.0,
                                                         static_cast<double>(secondaryDepthRange)));
            return stoneDepth <= 1 + offset + surfaceDepth + secondarySurfaceDepth;
        };
    };
}

const ConditionSource& surfaceRules::onFloor() {
    static const ConditionSource source = stoneDepthCheck(0, false, 0, false);
    return source;
}

const ConditionSource& surfaceRules::underFloor() {
    static const ConditionSource source = stoneDepthCheck(0, true, 0, false);
    return source;
}

const ConditionSource& surfaceRules::deepUnderFloor() {
    static const ConditionSource source = stoneDepthCheck(0, true, 6, false);
    return source;
}

const ConditionSource& surfaceRules::veryDeepUnderFloor() {
    static const ConditionSource source = stoneDepthCheck(0, true, 30, false);
    return source;
}

const ConditionSource& surfaceRules::onCeiling() {
    static const ConditionSource source = stoneDepthCheck(0, false, 0, true);
    return source;
}

const ConditionSource& surfaceRules::underCeiling() {
    static const ConditionSource source = stoneDepthCheck(0, true, 0, true);
    return source;
}

ConditionSource surfaceRules::notCondition(ConditionSource target) {
    return [target = std::move(target)](SurfaceRuleContext& context) -> Condition {
        Condition inner = target(context);
        return [inner = std::move(inner)] { return !inner(); };
    };
}

namespace {

[[nodiscard]] ConditionSource yCondition(VerticalAnchor anchor, std::int32_t surfaceDepthMultiplier,
                                         bool addStoneDepth) {
    return [anchor, surfaceDepthMultiplier, addStoneDepth](SurfaceRuleContext& context) -> Condition {
        const std::int32_t anchorY = anchor.resolveY(context.worldGenContext());
        SurfaceRuleContext* bound = &context;
        return [anchorY, surfaceDepthMultiplier, addStoneDepth, bound] {
            return bound->blockY() + (addStoneDepth ? bound->stoneDepthAbove() : 0) >=
                   anchorY + bound->surfaceDepth() * surfaceDepthMultiplier;
        };
    };
}

[[nodiscard]] ConditionSource waterCondition(std::int32_t offset, std::int32_t surfaceDepthMultiplier,
                                             bool addStoneDepth) {
    return [offset, surfaceDepthMultiplier, addStoneDepth](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [offset, surfaceDepthMultiplier, addStoneDepth, bound] {
            return bound->waterHeight() == std::numeric_limits<std::int32_t>::min() ||
                   bound->blockY() + (addStoneDepth ? bound->stoneDepthAbove() : 0) >=
                       bound->waterHeight() + offset + bound->surfaceDepth() * surfaceDepthMultiplier;
        };
    };
}

} // namespace

ConditionSource surfaceRules::yBlockCheck(VerticalAnchor anchor, std::int32_t surfaceDepthMultiplier) {
    return yCondition(anchor, surfaceDepthMultiplier, false);
}

ConditionSource surfaceRules::yStartCheck(VerticalAnchor anchor, std::int32_t surfaceDepthMultiplier) {
    return yCondition(anchor, surfaceDepthMultiplier, true);
}

ConditionSource surfaceRules::waterBlockCheck(std::int32_t offset, std::int32_t surfaceDepthMultiplier) {
    return waterCondition(offset, surfaceDepthMultiplier, false);
}

ConditionSource surfaceRules::waterStartCheck(std::int32_t offset, std::int32_t surfaceDepthMultiplier) {
    return waterCondition(offset, surfaceDepthMultiplier, true);
}

ConditionSource surfaceRules::isBiome(std::vector<std::string> target) {
    auto biomes = std::make_shared<const std::set<std::string, std::less<>>>(target.begin(), target.end());
    return [biomes = std::move(biomes)](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [biomes, bound] { return biomes->find(bound->biome()) != biomes->end(); };
    };
}

ConditionSource surfaceRules::noiseCondition2d(std::string noiseId, double minRange, double maxRange) {
    return [noiseId = std::move(noiseId), minRange, maxRange](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [noiseId, minRange, maxRange, bound] {
            const double value = bound->sample2d(noiseId);
            return value >= minRange && value <= maxRange;
        };
    };
}

ConditionSource surfaceRules::noiseCondition3d(std::string noiseId, double minRange, double maxRange) {
    return [noiseId = std::move(noiseId), minRange, maxRange](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [noiseId, minRange, maxRange, bound] {
            const double value = bound->sample3d(noiseId);
            return value >= minRange && value <= maxRange;
        };
    };
}

ConditionSource surfaceRules::verticalGradient(std::string randomName, VerticalAnchor trueAtAndBelow,
                                               VerticalAnchor falseAtAndAbove) {
    return [randomName = std::move(randomName), trueAtAndBelow,
            falseAtAndAbove](SurfaceRuleContext& context) -> Condition {
        const std::int32_t trueY = trueAtAndBelow.resolveY(context.worldGenContext());
        const std::int32_t falseY = falseAtAndAbove.resolveY(context.worldGenContext());
        SurfaceRuleContext* bound = &context;
        return [randomName, trueY, falseY, bound] {
            const std::int32_t y = bound->blockY();
            if (y <= trueY) {
                return true;
            }
            if (y >= falseY) {
                return false;
            }
            const double probability =
                mth::map(static_cast<double>(y), static_cast<double>(trueY), static_cast<double>(falseY), 1.0, 0.0);
            return static_cast<double>(bound->verticalGradientRandom(randomName)) < probability;
        };
    };
}

ConditionSource surfaceRules::steep() {
    return [](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [bound] { return bound->isSteep(); };
    };
}

ConditionSource surfaceRules::hole() {
    return [](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [bound] { return bound->isHole(); };
    };
}

ConditionSource surfaceRules::abovePreliminarySurface() {
    return [](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [bound] { return bound->isAbovePreliminarySurface(); };
    };
}

ConditionSource surfaceRules::temperature() {
    return [](SurfaceRuleContext& context) -> Condition {
        SurfaceRuleContext* bound = &context;
        return [bound] {
            return biomeTemperature::coldEnoughToSnow(bound->biome(), bound->blockX(), bound->blockY(),
                                                      bound->blockZ());
        };
    };
}

RuleSource surfaceRules::ifTrue(ConditionSource condition, RuleSource thenRun) {
    return
        [condition = std::move(condition), thenRun = std::move(thenRun)](SurfaceRuleContext& context) -> SurfaceRule {
            Condition builtCondition = condition(context);
            SurfaceRule builtRule = thenRun(context);
            return [builtCondition = std::move(builtCondition), builtRule = std::move(builtRule)](
                       std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                return builtCondition() ? builtRule(x, y, z) : std::nullopt;
            };
        };
}

RuleSource surfaceRules::sequence(std::vector<RuleSource> rules) {
    if (rules.empty()) {
        throw std::invalid_argument{"Need at least 1 rule for a sequence"};
    }
    return [rules = std::move(rules)](SurfaceRuleContext& context) -> SurfaceRule {
        auto built = std::make_shared<std::vector<SurfaceRule>>();
        built->reserve(rules.size());
        for (const RuleSource& rule : rules) {
            built->push_back(rule(context));
        }
        return
            [built = std::move(built)](std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<std::int32_t> {
                for (const SurfaceRule& rule : *built) {
                    if (std::optional<std::int32_t> result = rule(x, y, z); result.has_value()) {
                        return result;
                    }
                }
                return std::nullopt;
            };
    };
}

RuleSource surfaceRules::state(std::int32_t stateId) {
    return [stateId](SurfaceRuleContext&) -> SurfaceRule {
        return [stateId](std::int32_t, std::int32_t, std::int32_t) -> std::optional<std::int32_t> { return stateId; };
    };
}

RuleSource surfaceRules::bandlands() {
    return [](SurfaceRuleContext& context) -> SurfaceRule {
        SurfaceRuleContext* bound = &context;
        return [bound](std::int32_t, std::int32_t, std::int32_t) -> std::optional<std::int32_t> {
            return bound->clayBand();
        };
    };
}

} // namespace basalt::server::world::terrain::surface
