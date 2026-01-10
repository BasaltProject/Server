#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/ConcentricRingsPlacement.hpp>
#include <basalt/wire/common/JavaCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace basalt::server::world::structure {
namespace {

constexpr std::int32_t kSearchRadius = 112;

} // namespace

std::vector<std::pair<std::int32_t, std::int32_t>>
ConcentricRingsPlacement::ringPositions(const std::int64_t seed) const {
    if (count_ == 0) {
        return {};
    }
    /// parity: `BiomeSource.findBiomeHorizontal` with `findClosest` false, which is not a spiral out
    /// from the centre: it sweeps the whole square at the outermost radius once and keeps a uniform
    /// pick among every cell that matched, by reservoir sampling.
    const auto search = [this](std::int32_t chunkX, std::int32_t chunkZ,
                               random::RandomSource& random) -> std::optional<std::pair<std::int32_t, std::int32_t>> {
        const std::int32_t originX = (chunkX << 4) + 8;
        const std::int32_t originZ = (chunkZ << 4) + 8;
        const std::int32_t centreQuartX = originX >> 2;
        const std::int32_t centreQuartZ = originZ >> 2;
        const std::int32_t radius = kSearchRadius >> 2;
        std::int32_t resultX = 0;
        std::int32_t resultZ = 0;
        std::int32_t found = 0;
        for (std::int32_t dz = -radius; dz <= radius; ++dz) {
            for (std::int32_t dx = -radius; dx <= radius; ++dx) {
                const std::int32_t quartX = centreQuartX + dx;
                const std::int32_t quartZ = centreQuartZ + dz;
                if (!preferredBiomes_.contains(noiseBiomeAt_(quartX, 0, quartZ))) {
                    continue;
                }
                if (found == 0 || random.nextInt(found + 1) == 0) {
                    resultX = quartX << 2;
                    resultZ = quartZ << 2;
                }
                ++found;
            }
        }
        if (found == 0) {
            return std::nullopt;
        }
        return std::pair{resultX >> 4, resultZ >> 4};
    };

    random::RandomSource random = random::RandomSource::legacy(0);
    random.setSeed(seed);
    std::vector<std::pair<std::int32_t, std::int32_t>> positions;
    positions.reserve(static_cast<std::size_t>(count_));
    double angle = random.nextDouble() * std::numbers::pi * 2.0;
    std::int32_t positionInCircle = 0;
    std::int32_t circle = 0;
    std::int32_t ringSpread = spread_;
    for (std::int32_t index = 0; index < count_; ++index) {
        const double ringDistance =
            4.0 * distance_ + distance_ * circle * 6.0 + (random.nextDouble() - 0.5) * (distance_ * 2.5);
        // parity: `Math.round` is floor(v + 0.5), which for a negative half lands one higher than a
        // round-half-away-from-zero would.
        const auto initialX = wire::common::toIntLikeJava(std::floor(std::cos(angle) * ringDistance + 0.5));
        const auto initialZ = wire::common::toIntLikeJava(std::floor(std::sin(angle) * ringDistance + 0.5));
        // parity: the fork is drawn here, in the main loop, even though the search that uses it runs
        // on another thread; that is what keeps the list reproducible.
        random::RandomSource searchRandom = random.fork();
        positions.push_back(search(initialX, initialZ, searchRandom).value_or(std::pair{initialX, initialZ}));
        angle += std::numbers::pi * 2.0 / ringSpread;
        if (++positionInCircle == ringSpread) {
            ++circle;
            positionInCircle = 0;
            ringSpread += 2 * ringSpread / (circle + 1);
            ringSpread = std::min(ringSpread, count_ - index);
            angle += random.nextDouble() * std::numbers::pi * 2.0;
        }
    }
    return positions;
}

} // namespace basalt::server::world::structure
