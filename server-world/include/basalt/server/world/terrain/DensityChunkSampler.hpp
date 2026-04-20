#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain {

class DensityChunkSampler {
public:
    DensityChunkSampler(const NoiseRouter& router, std::int32_t chunkX, std::int32_t chunkZ,
                        densityfunction::Sampler beardifier = nullptr);

    [[nodiscard]] double sample(std::int32_t localX, std::int32_t y, std::int32_t localZ) const;

    [[nodiscard]] std::int32_t minY() const noexcept { return minY_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

private:
    std::int32_t minY_;
    std::int32_t height_;
    std::vector<float> values_;
};

} // namespace basalt::server::world::terrain
