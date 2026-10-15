#pragma once

#include <cstdint>
#include <limits>
#include <utility>

namespace basalt::server::world::terrain {

/// parity: `BiomeManager.getBiome` and its seed-fiddled three-dimensional Voronoi selection.
class BiomeZoomer {
public:
    explicit constexpr BiomeZoomer(std::int64_t seed) noexcept : seed_(seed) {}

    template <typename Lookup>
    [[nodiscard]] decltype(auto) biomeAt(std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                         Lookup&& noiseBiomeAt) const {
        const std::int32_t absoluteX = blockX - 2;
        const std::int32_t absoluteY = blockY - 2;
        const std::int32_t absoluteZ = blockZ - 2;
        const std::int32_t parentX = absoluteX >> 2;
        const std::int32_t parentY = absoluteY >> 2;
        const std::int32_t parentZ = absoluteZ >> 2;
        const double fractionX = static_cast<double>(absoluteX & 3) / 4.0;
        const double fractionY = static_cast<double>(absoluteY & 3) / 4.0;
        const double fractionZ = static_cast<double>(absoluteZ & 3) / 4.0;
        std::int32_t closestIndex = 0;
        double closestDistance = std::numeric_limits<double>::infinity();

        for (std::int32_t index = 0; index < 8; ++index) {
            const bool lowerX = (index & 4) == 0;
            const bool lowerY = (index & 2) == 0;
            const bool lowerZ = (index & 1) == 0;
            const double distance =
                fiddledDistance(seed_, lowerX ? parentX : parentX + 1, lowerY ? parentY : parentY + 1,
                                lowerZ ? parentZ : parentZ + 1, lowerX ? fractionX : fractionX - 1.0,
                                lowerY ? fractionY : fractionY - 1.0, lowerZ ? fractionZ : fractionZ - 1.0);
            if (closestDistance > distance) {
                closestIndex = index;
                closestDistance = distance;
            }
        }

        return std::forward<Lookup>(noiseBiomeAt)((closestIndex & 4) == 0 ? parentX : parentX + 1,
                                                  (closestIndex & 2) == 0 ? parentY : parentY + 1,
                                                  (closestIndex & 1) == 0 ? parentZ : parentZ + 1);
    }

    [[nodiscard]] static double fiddledDistance(std::int64_t seed, std::int32_t quartX, std::int32_t quartY,
                                                std::int32_t quartZ, double distanceX, double distanceY,
                                                double distanceZ) noexcept;

private:
    std::int64_t seed_;
};

} // namespace basalt::server::world::terrain
