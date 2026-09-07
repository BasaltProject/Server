#include <basalt/server/world/terrain/StructureBeardifier.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>

namespace basalt::server::world::terrain {
namespace {

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

/// parity: `Beardifier.getBuryContribution`, a straight falloff from 1 to 0 over six blocks.
[[nodiscard]] double buryContribution(double dx, double dy, double dz) {
    return wire::common::mth::clampedMap(std::sqrt(dx * dx + dy * dy + dz * dz), 0.0, 6.0, 1.0, 0.0);
}

[[nodiscard]] double beardContribution(std::int32_t dx, std::int32_t dy, std::int32_t dz, std::int32_t yToGround) {
    const std::int32_t xi = dx + kKernelRadius;
    const std::int32_t yi = dy + kKernelRadius;
    const std::int32_t zi = dz + kKernelRadius;
    if (xi < 0 || xi >= kKernelSize || yi < 0 || yi >= kKernelSize || zi < 0 || zi >= kKernelSize) {
        return 0.0;
    }
    const double offsetY = yToGround + 0.5;
    const double distanceSquared = dx * dx + offsetY * offsetY + dz * dz;
    const double value = -offsetY * wire::common::mth::fastInvSqrt(distanceSquared / 2.0) / 2.0;
    return value * static_cast<double>(
                       kernel()[static_cast<std::size_t>(zi * kKernelSize * kKernelSize + xi * kKernelSize + yi)]);
}

class Beardifier final : public densityfunction::DensityFunction {
public:
    Beardifier(std::vector<BeardifierRigid> pieces, std::vector<BeardifierJunction> junctions)
        : pieces_(std::move(pieces)), junctions_(std::move(junctions)) {}

    [[nodiscard]] double compute(densityfunction::FunctionContext& context) const override {
        double density = 0.0;
        for (const BeardifierRigid& rigid : pieces_) {
            if (rigid.adjustment == TerrainAdjustment::None) {
                continue;
            }
            const structure::StructureBoundingBox& box = rigid.box;
            const std::int32_t dx = std::max(0, std::max(box.minX - context.blockX, context.blockX - box.maxX));
            const std::int32_t dz = std::max(0, std::max(box.minZ - context.blockZ, context.blockZ - box.maxZ));
            const std::int32_t groundY = box.minY + rigid.groundLevelDelta;
            const std::int32_t dyToGround = context.blockY - groundY;
            std::int32_t dy = dyToGround;
            if (rigid.adjustment == TerrainAdjustment::BeardBox) {
                // A box clears the piece's whole height instead of bearding up from its floor, which
                // is what lets the ancient city sit in a hollow rather than in solid rock.
                dy = std::max(0, std::max(groundY - context.blockY, context.blockY - box.maxY));
            } else if (rigid.adjustment == TerrainAdjustment::Encapsulate) {
                dy = std::max(0, std::max(box.minY - context.blockY, context.blockY - box.maxY));
            }
            if (rigid.adjustment == TerrainAdjustment::Encapsulate) {
                // parity: encapsulate ADDS density instead of carving, so the structure ends up
                // cased in rock rather than sitting in a hollow. Halving each axis first is what
                // stretches that casing out to roughly twelve blocks.
                density += buryContribution(dx / 2.0, dy / 2.0, dz / 2.0) * 0.8;
            } else if (rigid.adjustment == TerrainAdjustment::Bury) {
                // parity: bury halves only the vertical axis and takes no 0.8, so it fills the
                // ground back over the piece rather than clearing a hollow under it.
                density += buryContribution(dx, dy / 2.0, dz);
            } else {
                density += beardContribution(dx, dy, dz, dyToGround) * 0.8;
            }
        }
        for (const BeardifierJunction& junction : junctions_) {
            const std::int32_t dx = context.blockX - junction.x;
            const std::int32_t dy = context.blockY - junction.groundY;
            const std::int32_t dz = context.blockZ - junction.z;
            density += beardContribution(dx, dy, dz, dy) * 0.4;
        }
        return density;
    }

    [[nodiscard]] double minValue() const override { return -std::numeric_limits<double>::infinity(); }

    [[nodiscard]] double maxValue() const override { return std::numeric_limits<double>::infinity(); }

private:
    std::vector<BeardifierRigid> pieces_;
    std::vector<BeardifierJunction> junctions_;
};

} // namespace

densityfunction::Function structureBeardifier(std::vector<BeardifierRigid> pieces,
                                              std::vector<BeardifierJunction> junctions) {
    return std::make_shared<const Beardifier>(std::move(pieces), std::move(junctions));
}

} // namespace basalt::server::world::terrain
