#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/wire/common/JavaCast.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::world::climate {
namespace {

constexpr std::int64_t kMaxRadius = 2048;

struct Candidate {
    std::int32_t x = 0;
    std::int32_t z = 0;
    std::int64_t fitness = 0;
};

/// parity: `getSpawnPositionAndFitness`. The sample is taken at y 0 and its depth is then
/// **overwritten with zero**, so the target's own depth span is what decides, not the terrain's. The
/// distance to the origin is added afterwards, scaled so that climate always outranks it.
[[nodiscard]] Candidate fitnessAt(const Sampler& sampler, const std::vector<ParameterPoint>& targets,
                                  std::int32_t blockX, std::int32_t blockZ) {
    const TargetPoint sampled = sampler.sample(blockX >> 2, 0, blockZ >> 2);
    const TargetPoint flat{.temperature = sampled.temperature,
                           .humidity = sampled.humidity,
                           .continentalness = sampled.continentalness,
                           .erosion = sampled.erosion,
                           .depth = 0,
                           .weirdness = sampled.weirdness};
    std::int64_t minFitness = std::numeric_limits<std::int64_t>::max();
    for (const ParameterPoint& target : targets) {
        minFitness = std::min(minFitness, target.fitness(flat));
    }
    const std::int64_t distanceBias = wire::common::mth::square(blockX) + wire::common::mth::square(blockZ);
    return Candidate{
        .x = blockX, .z = blockZ, .fitness = minFitness * wire::common::mth::square(kMaxRadius) + distanceBias};
}

[[nodiscard]] Candidate radialSearch(const Sampler& sampler, const std::vector<ParameterPoint>& targets,
                                     const Candidate& from, float maxRadius, float increment) {
    Candidate best = from;
    float angle = 0.0F;
    float radius = increment;
    // parity: the spiral is anchored where this search *started*, not where it is winning, so
    // improving mid-spiral does not drag the remaining samples along with it.
    const std::int32_t originX = from.x;
    const std::int32_t originZ = from.z;
    while (radius <= maxRadius) {
        const std::int32_t x =
            originX + wire::common::toIntLikeJava(std::sin(static_cast<double>(angle)) * static_cast<double>(radius));
        const std::int32_t z =
            originZ + wire::common::toIntLikeJava(std::cos(static_cast<double>(angle)) * static_cast<double>(radius));
        const Candidate candidate = fitnessAt(sampler, targets, x, z);
        if (candidate.fitness < best.fitness) {
            best = candidate;
        }
        angle += increment / radius;
        if (angle > static_cast<float>(std::numbers::pi * 2)) {
            angle = 0.0F;
            radius += increment;
        }
    }
    return best;
}

} // namespace

const std::vector<ParameterPoint>& spawnFinder::overworldTarget() {
    static const std::vector<ParameterPoint> targets = {ParameterPoint{.temperature = parameterSpan(-1.0F, 1.0F),
                                                                       .humidity = parameterSpan(-1.0F, 1.0F),
                                                                       .continentalness = parameterSpan(-0.11F, 1.0F),
                                                                       .erosion = parameterSpan(-1.0F, 1.0F),
                                                                       .depth = parameterPointAt(0.0F),
                                                                       .weirdness = parameterSpan(-1.0F, -0.16F),
                                                                       .offset = quantizeCoord(0.0F)},
                                                        ParameterPoint{.temperature = parameterSpan(-1.0F, 1.0F),
                                                                       .humidity = parameterSpan(-1.0F, 1.0F),
                                                                       .continentalness = parameterSpan(-0.11F, 1.0F),
                                                                       .erosion = parameterSpan(-1.0F, 1.0F),
                                                                       .depth = parameterPointAt(0.0F),
                                                                       .weirdness = parameterSpan(0.16F, 1.0F),
                                                                       .offset = quantizeCoord(0.0F)}};
    return targets;
}

std::pair<std::int32_t, std::int32_t> spawnFinder::findSpawnPosition(const Sampler& sampler,
                                                                     const std::vector<ParameterPoint>& targets) {
    if (targets.empty()) {
        return {0, 0};
    }
    Candidate result = fitnessAt(sampler, targets, 0, 0);
    result = radialSearch(sampler, targets, result, 2048.0F, 512.0F);
    result = radialSearch(sampler, targets, result, 512.0F, 32.0F);
    return {result.x, result.z};
}

std::pair<std::int32_t, std::int32_t> spawnFinder::findSpawnPosition(const Sampler& sampler) {
    return findSpawnPosition(sampler, overworldTarget());
}

} // namespace basalt::server::world::climate
