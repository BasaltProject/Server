#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <cstdint>

namespace basalt::server::world::densityfunction {
class RandomState;
class SamplerContext;
} // namespace basalt::server::world::densityfunction

namespace basalt::server::world::climate {

/// parity: `Climate.Sampler`. Samples the six axes at a position given in quart coordinates,
/// vanilla's four-block biome grid, and quantises the result.
class Sampler {
public:
    Sampler(densityfunction::RandomState& state, const densityfunction::Function& temperature,
            const densityfunction::Function& humidity, const densityfunction::Function& continentalness,
            const densityfunction::Function& erosion, const densityfunction::Function& depth,
            const densityfunction::Function& weirdness);

    /// Through `context` when given, otherwise uncached, the way `createUncachedResolver` samples.
    [[nodiscard]] TargetPoint sample(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                     densityfunction::SamplerContext* context = nullptr) const;

private:
    densityfunction::Sampler temperature_;
    densityfunction::Sampler humidity_;
    densityfunction::Sampler continentalness_;
    densityfunction::Sampler erosion_;
    densityfunction::Sampler depth_;
    densityfunction::Sampler weirdness_;
};

} // namespace basalt::server::world::climate
