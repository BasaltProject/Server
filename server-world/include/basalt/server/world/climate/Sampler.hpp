#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <cstdint>

namespace basalt::server::world::climate {

/// parity: `Climate.Sampler`. Samples the six axes at a position given in quart coordinates,
/// vanilla's four-block biome grid, and quantises the result.
class Sampler {
public:
    Sampler(densityfunction::Function temperature, densityfunction::Function humidity,
            densityfunction::Function continentalness, densityfunction::Function erosion,
            densityfunction::Function depth, densityfunction::Function weirdness)
        : temperature_(std::move(temperature)), humidity_(std::move(humidity)),
          continentalness_(std::move(continentalness)), erosion_(std::move(erosion)), depth_(std::move(depth)),
          weirdness_(std::move(weirdness)) {}

    [[nodiscard]] TargetPoint sample(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                     densityfunction::DensityCache* cache = nullptr) const;

private:
    densityfunction::Function temperature_;
    densityfunction::Function humidity_;
    densityfunction::Function continentalness_;
    densityfunction::Function erosion_;
    densityfunction::Function depth_;
    densityfunction::Function weirdness_;
};

} // namespace basalt::server::world::climate
