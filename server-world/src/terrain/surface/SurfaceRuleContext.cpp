#include <basalt/server/world/terrain/surface/SurfaceRuleContext.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>

namespace basalt::server::world::terrain::surface {
namespace mth = wire::common::mth;

void SurfaceRuleContext::updateXZ(std::int32_t x, std::int32_t z) {
    blockX_ = x;
    blockZ_ = z;
    surfaceDepth_ = environment_->surfaceDepthNoise(x, z);
    estimatedSurfaceHeight_ = environment_->estimatedSurfaceHeight(x, z);
    const std::int32_t cellX = x >> 4;
    const std::int32_t cellZ = z >> 4;
    preliminaryCorners_[0] = environment_->preliminarySurfaceLevel(cellX << 4, cellZ << 4);
    preliminaryCorners_[1] = environment_->preliminarySurfaceLevel((cellX + 1) << 4, cellZ << 4);
    preliminaryCorners_[2] = environment_->preliminarySurfaceLevel(cellX << 4, (cellZ + 1) << 4);
    preliminaryCorners_[3] = environment_->preliminarySurfaceLevel((cellX + 1) << 4, (cellZ + 1) << 4);
}

std::int32_t SurfaceRuleContext::minSurfaceLevel() const {
    const double alongX = static_cast<double>(blockX_ & 15) / 16.0;
    const double alongZ = static_cast<double>(blockZ_ & 15) / 16.0;
    const double interpolated = mth::lerp2(
        alongX, alongZ, static_cast<double>(preliminaryCorners_[0]), static_cast<double>(preliminaryCorners_[1]),
        static_cast<double>(preliminaryCorners_[2]), static_cast<double>(preliminaryCorners_[3]));
    return static_cast<std::int32_t>(std::floor(interpolated)) + surfaceDepth_ - 8;
}

} // namespace basalt::server::world::terrain::surface
