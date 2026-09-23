#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/JavaMath.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

namespace jm = noise::javaMath;

constexpr std::int32_t kKernelRadius = 12;
constexpr std::int32_t kKernelSize = 24;

const std::array<float, kKernelSize * kKernelSize * kKernelSize>& kernel() {
    static const std::array<float, kKernelSize * kKernelSize * kKernelSize> built = [] {
        std::array<float, kKernelSize * kKernelSize * kKernelSize> values{};
        for (std::size_t index = 0; index < values.size(); ++index) {
            const auto step = static_cast<std::int32_t>(index);
            const std::int32_t z = step / (kKernelSize * kKernelSize) - kKernelRadius;
            const std::int32_t x = step / kKernelSize % kKernelSize - kKernelRadius;
            const std::int32_t y = step % kKernelSize - kKernelRadius;
            const double exponent = -(x * x + (y + 0.5) * (y + 0.5) + z * z) / 16.0;
            values[index] = static_cast<float>(std::pow(std::numbers::e, exponent));
        }
        return values;
    }();
    return built;
}

/// parity: `Beardifier.getBuryContribution`, in float since 26.3.
[[nodiscard]] float buryContribution(float dx, float dy, float dz) {
    const float distanceSquared = dx * dx + dy * dy + dz * dz;
    return distanceSquared >= 36.0F ? 0.0F
                                    : 1.0F - static_cast<float>(std::sqrt(static_cast<double>(distanceSquared))) / 6.0F;
}

[[nodiscard]] float beardContribution(std::int32_t dx, std::int32_t dy, std::int32_t dz, std::int32_t yToGround) {
    const std::int32_t xi = dx + kKernelRadius;
    const std::int32_t yi = dy + kKernelRadius;
    const std::int32_t zi = dz + kKernelRadius;
    if (xi < 0 || xi >= kKernelSize || yi < 0 || yi >= kKernelSize || zi < 0 || zi >= kKernelSize) {
        return 0.0F;
    }
    const float offsetY = static_cast<float>(yToGround) + 0.5F;
    const float distanceSquared = static_cast<float>(dx) * static_cast<float>(dx) + offsetY * offsetY +
                                  static_cast<float>(dz) * static_cast<float>(dz);
    const float value =
        -offsetY * static_cast<float>(wire::common::mth::fastInvSqrt(static_cast<double>(distanceSquared / 2.0F))) /
        2.0F;
    return value * kernel()[static_cast<std::size_t>(zi * kKernelSize * kKernelSize + xi * kKernelSize + yi)];
}

class Beardifier final : public densityfunction::DensitySampler {
public:
    Beardifier(StructureBeard beard, structure::StructureBoundingBox affected)
        : beard_(std::move(beard)), affected_(affected) {}

    [[nodiscard]] float sampleValue(densityfunction::SamplerContext&, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return inside(x, y, z) ? sampleUnchecked(x, y, z) : 0.0F;
    }

    void sampleVolume(densityfunction::SamplerContext&, densityfunction::DensityBuffer& output,
                      const densityfunction::DensityVolume& volume) const override {
        output.fill(0.0F);
        if (affected_.minX > volume.maxBlockX() || affected_.maxX < volume.minBlockX ||
            affected_.minY > volume.maxBlockY() || affected_.maxY < volume.minBlockY ||
            affected_.minZ > volume.maxBlockZ() || affected_.maxZ < volume.minBlockZ) {
            return;
        }
        const std::int32_t minX = jm::floorDiv(std::max(0, affected_.minX - volume.minBlockX), volume.stepBlockX);
        const std::int32_t minY = jm::floorDiv(std::max(0, affected_.minY - volume.minBlockY), volume.stepBlockY);
        const std::int32_t minZ = jm::floorDiv(std::max(0, affected_.minZ - volume.minBlockZ), volume.stepBlockZ);
        const std::int32_t maxX =
            std::min(volume.sizeX - 1, jm::floorDiv(affected_.maxX - volume.minBlockX, volume.stepBlockX));
        const std::int32_t maxY =
            std::min(volume.sizeY - 1, jm::floorDiv(affected_.maxY - volume.minBlockY, volume.stepBlockY));
        const std::int32_t maxZ =
            std::min(volume.sizeZ - 1, jm::floorDiv(affected_.maxZ - volume.minBlockZ, volume.stepBlockZ));
        for (std::int32_t z = minZ; z <= maxZ; ++z) {
            for (std::int32_t x = minX; x <= maxX; ++x) {
                for (std::int32_t y = minY; y <= maxY; ++y) {
                    output.set(volume.indexUnchecked(x, y, z),
                               sampleUnchecked(volume.blockX(x), volume.blockY(y), volume.blockZ(z)));
                }
            }
        }
    }

private:
    [[nodiscard]] bool inside(std::int32_t x, std::int32_t y, std::int32_t z) const noexcept {
        return x >= affected_.minX && x <= affected_.maxX && y >= affected_.minY && y <= affected_.maxY &&
               z >= affected_.minZ && z <= affected_.maxZ;
    }

    [[nodiscard]] float sampleUnchecked(std::int32_t x, std::int32_t y, std::int32_t z) const {
        float density = 0.0F;
        for (const BeardifierRigid& rigid : beard_.pieces) {
            const structure::StructureBoundingBox& box = rigid.box;
            const std::int32_t dx = std::max(0, std::max(box.minX - x, x - box.maxX));
            const std::int32_t dz = std::max(0, std::max(box.minZ - z, z - box.maxZ));
            const std::int32_t groundY = box.minY + rigid.groundLevelDelta;
            const std::int32_t dyToGround = y - groundY;
            std::int32_t dy = 0;
            switch (rigid.adjustment) {
            case TerrainAdjustment::None:
                break;
            case TerrainAdjustment::Bury:
            case TerrainAdjustment::BeardThin:
                dy = dyToGround;
                break;
            case TerrainAdjustment::BeardBox:
                dy = std::max(0, std::max(groundY - y, y - box.maxY));
                break;
            case TerrainAdjustment::Encapsulate:
                dy = std::max(0, std::max(box.minY - y, y - box.maxY));
                break;
            }
            float contribution = 0.0F;
            switch (rigid.adjustment) {
            case TerrainAdjustment::None:
                break;
            case TerrainAdjustment::Bury:
                contribution =
                    buryContribution(static_cast<float>(dx), static_cast<float>(dy) / 2.0F, static_cast<float>(dz));
                break;
            case TerrainAdjustment::BeardThin:
            case TerrainAdjustment::BeardBox:
                contribution = beardContribution(dx, dy, dz, dyToGround) * 0.8F;
                break;
            case TerrainAdjustment::Encapsulate:
                contribution = buryContribution(static_cast<float>(dx) / 2.0F, static_cast<float>(dy) / 2.0F,
                                                static_cast<float>(dz) / 2.0F) *
                               0.8F;
                break;
            }
            density += contribution;
        }
        for (const BeardifierJunction& junction : beard_.junctions) {
            const std::int32_t dx = x - junction.x;
            const std::int32_t dy = y - junction.groundY;
            const std::int32_t dz = z - junction.z;
            density += beardContribution(dx, dy, dz, dy) * 0.4F;
        }
        return density;
    }

    StructureBeard beard_;
    structure::StructureBoundingBox affected_;
};

} // namespace

densityfunction::Sampler structureBeardifier(StructureBeard beard) {
    if (beard.empty()) {
        return nullptr;
    }
    std::optional<structure::StructureBoundingBox> box;
    const auto include = [&box](const structure::StructureBoundingBox& next) {
        if (!box.has_value()) {
            box = next;
            return;
        }
        box = structure::StructureBoundingBox{std::min(box->minX, next.minX), std::min(box->minY, next.minY),
                                              std::min(box->minZ, next.minZ), std::max(box->maxX, next.maxX),
                                              std::max(box->maxY, next.maxY), std::max(box->maxZ, next.maxZ)};
    };
    for (const BeardifierRigid& rigid : beard.pieces) {
        include(rigid.box);
    }
    for (const BeardifierJunction& junction : beard.junctions) {
        include(structure::StructureBoundingBox{junction.x, junction.groundY, junction.z, junction.x, junction.groundY,
                                                junction.z});
    }
    // parity: the affected box is everything the pieces touch, inflated by 24.
    const structure::StructureBoundingBox affected{box->minX - 24, box->minY - 24, box->minZ - 24,
                                                   box->maxX + 24, box->maxY + 24, box->maxZ + 24};
    return std::make_shared<const Beardifier>(std::move(beard), affected);
}

} // namespace basalt::server::world::terrain
