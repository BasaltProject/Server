#include "FrozenOceanShared.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/wire/common/JavaCast.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::world::terrain::decoration::frozenocean {
namespace {

constexpr DecorationPos kZero{0, 0, 0};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

[[nodiscard]] bool isIcebergState(std::int32_t stateId) {
    return stateId == stateIdOf("packed_ice") || stateId == stateIdOf("snow_block") || stateId == stateIdOf("blue_ice");
}

/// parity: `Float.pow(Int)`, which goes through the double-precision `Math.pow` and back.
[[nodiscard]] float squared(float value) { return static_cast<float>(std::pow(static_cast<double>(value), 2.0)); }

[[nodiscard]] double signedDistanceEllipse(std::int32_t xOffset, std::int32_t zOffset, const DecorationPos& origin,
                                           std::int32_t a, std::int32_t c, double angle) {
    const double dx = xOffset - origin.x;
    const double dz = zOffset - origin.z;
    return std::pow((dx * std::cos(angle) - dz * std::sin(angle)) / a, 2.0) +
           std::pow((dx * std::sin(angle) + dz * std::cos(angle)) / c, 2.0) - 1.0;
}

[[nodiscard]] double signedDistanceCircle(std::int32_t xOffset, std::int32_t zOffset, const DecorationPos& origin,
                                          std::int32_t radius, random::RandomSource& random) {
    const float offset = 10.0F * std::clamp(random.nextFloat(), 0.2F, 0.8F) / static_cast<float>(radius);
    return static_cast<double>(offset) + std::pow(static_cast<double>(xOffset - origin.x), 2.0) +
           std::pow(static_cast<double>(zOffset - origin.z), 2.0) - std::pow(static_cast<double>(radius), 2.0);
}

[[nodiscard]] std::int32_t heightDependentRadiusRound(random::RandomSource& random, std::int32_t yOffset,
                                                      std::int32_t height, std::int32_t width) {
    const float k = 3.5F - random.nextFloat();
    float scale =
        (1.0F - squared(static_cast<float>(yOffset)) / (static_cast<float>(height) * k)) * static_cast<float>(width);
    if (height > 15 + random.nextInt(5)) {
        const std::int32_t adjustedY = yOffset < 3 + random.nextInt(6) ? yOffset / 2 : yOffset;
        scale = (1.0F - static_cast<float>(adjustedY) / (static_cast<float>(height) * k * 0.4F)) *
                static_cast<float>(width);
    }
    return wire::common::toIntLikeJava(static_cast<double>(std::ceil(scale / 2.0F)));
}

[[nodiscard]] std::int32_t heightDependentRadiusEllipse(std::int32_t yOffset, std::int32_t height, std::int32_t width) {
    const float scale =
        (1.0F - squared(static_cast<float>(yOffset)) / static_cast<float>(height)) * static_cast<float>(width);
    return wire::common::toIntLikeJava(static_cast<double>(std::ceil(scale / 2.0F)));
}

[[nodiscard]] std::int32_t heightDependentRadiusSteep(random::RandomSource& random, std::int32_t yOffset,
                                                      std::int32_t height, std::int32_t width) {
    const float k = 1.0F + random.nextFloat() / 2.0F;
    const float scale =
        (1.0F - static_cast<float>(yOffset) / (static_cast<float>(height) * k)) * static_cast<float>(width);
    return wire::common::toIntLikeJava(static_cast<double>(std::ceil(scale / 2.0F)));
}

[[nodiscard]] std::int32_t ellipseCAt(std::int32_t yOffset, std::int32_t height, std::int32_t ellipseC) {
    return yOffset > 0 && height - yOffset <= 3 ? ellipseC - (4 - (height - yOffset)) : ellipseC;
}

void setIcebergBlock(const DecorationPos& position, ChunkFeatureRegion& region, random::RandomSource& random,
                     std::int32_t heightDifference, std::int32_t height, bool ellipse, bool snowOnTop,
                     std::int32_t mainStateId) {
    const std::int32_t stateId = region.get(position);
    if (!registry::blocks::isAir(stateId) && stateId != stateIdOf("snow_block") && stateId != stateIdOf("ice") &&
        !isFrozenWater(stateId)) {
        return;
    }
    const bool randomness = !ellipse || random.nextDouble() > 0.05;
    const std::int32_t divisor = ellipse ? 3 : 2;
    if (snowOnTop && !isFrozenWater(stateId) &&
        static_cast<double>(heightDifference) <= random.nextInt(std::max(1, height / divisor)) + height * 0.6 &&
        randomness) {
        region.set(position, stateIdOf("snow_block"));
    } else {
        region.set(position, mainStateId);
    }
}

void generateBlock(ChunkFeatureRegion& region, random::RandomSource& random, const DecorationPos& origin,
                   std::int32_t height, std::int32_t xOffset, std::int32_t yOffset, std::int32_t zOffset,
                   std::int32_t radius, std::int32_t a, bool ellipse, std::int32_t ellipseC, double shapeAngle,
                   bool snowOnTop, std::int32_t mainStateId) {
    const double signedDistance =
        ellipse ? signedDistanceEllipse(xOffset, zOffset, kZero, a, ellipseCAt(yOffset, height, ellipseC), shapeAngle)
                : signedDistanceCircle(xOffset, zOffset, kZero, radius, random);
    if (signedDistance >= 0.0) {
        return;
    }
    const DecorationPos position = offsetBy(origin, xOffset, yOffset, zOffset);
    const double compareValue = ellipse ? -0.5 : static_cast<double>(-6 - random.nextInt(3));
    if (signedDistance > compareValue && random.nextDouble() > 0.9) {
        return;
    }
    setIcebergBlock(position, region, random, height - yOffset, height, ellipse, snowOnTop, mainStateId);
}

void smooth(ChunkFeatureRegion& region, const DecorationPos& origin, std::int32_t width, std::int32_t height,
            bool ellipse, std::int32_t ellipseA) {
    const std::int32_t a = ellipse ? ellipseA : width / 2;
    for (std::int32_t xOffset = -a; xOffset <= a; ++xOffset) {
        for (std::int32_t zOffset = -a; zOffset <= a; ++zOffset) {
            for (std::int32_t yOffset = 0; yOffset <= height; ++yOffset) {
                const DecorationPos position = offsetBy(origin, xOffset, yOffset, zOffset);
                const std::int32_t stateId = region.get(position);
                if (!isIcebergState(stateId) && stateId != stateIdOf("snow")) {
                    continue;
                }
                if (registry::blocks::isAir(region.get(down(position)))) {
                    region.set(position, registry::blocks::air().stateId);
                    region.set(up(position), registry::blocks::air().stateId);
                } else if (isIcebergState(stateId)) {
                    std::int32_t nonIcebergSides = 0;
                    for (std::size_t index = 2; index < kAllDirections.size(); ++index) {
                        if (!isIcebergState(region.get(offsetBy(position, kAllDirections[index])))) {
                            ++nonIcebergSides;
                        }
                    }
                    if (nonIcebergSides >= 3) {
                        region.set(position, registry::blocks::air().stateId);
                    }
                }
            }
        }
    }
}

void carve(std::int32_t radius, std::int32_t yOffset, const DecorationPos& globalOrigin, ChunkFeatureRegion& region,
           bool underWater, double angle, const DecorationPos& localOrigin, std::int32_t ellipseA,
           std::int32_t ellipseC) {
    const std::int32_t a = radius + 1 + ellipseA / 3;
    const std::int32_t c = std::min(radius - 3, 3) + ellipseC / 2 - 1;
    for (std::int32_t xOffset = -a; xOffset < a; ++xOffset) {
        for (std::int32_t zOffset = -a; zOffset < a; ++zOffset) {
            // parity: a zero minor axis makes the distance NaN, and vanilla's `< 0.0` rejects it.
            // The inverted `>= 0.0` would let it through.
            if (!(signedDistanceEllipse(xOffset, zOffset, localOrigin, a, c, angle) < 0.0)) {
                continue;
            }
            const DecorationPos position = offsetBy(globalOrigin, xOffset, yOffset, zOffset);
            const std::int32_t stateId = region.get(position);
            if (!isIcebergState(stateId) && stateId != stateIdOf("snow_block")) {
                continue;
            }
            if (underWater) {
                region.set(position, registry::blocks::water().stateId);
            } else {
                region.set(position, registry::blocks::air().stateId);
                if (region.get(up(position)) == stateIdOf("snow")) {
                    region.set(up(position), registry::blocks::air().stateId);
                }
            }
        }
    }
}

void generateCutOut(random::RandomSource& random, ChunkFeatureRegion& region, std::int32_t width, std::int32_t height,
                    const DecorationPos& globalOrigin, bool ellipse, std::int32_t ellipseA, double shapeAngle,
                    std::int32_t ellipseC) {
    const std::int32_t signX = random.nextBoolean() ? -1 : 1;
    const std::int32_t signZ = random.nextBoolean() ? -1 : 1;
    std::int32_t xOffset = random.nextInt(std::max(width / 2 - 2, 1));
    if (random.nextBoolean()) {
        xOffset = width / 2 + 1 - random.nextInt(std::max(width - width / 2 - 1, 1));
    }
    std::int32_t zOffset = random.nextInt(std::max(width / 2 - 2, 1));
    if (random.nextBoolean()) {
        zOffset = width / 2 + 1 - random.nextInt(std::max(width - width / 2 - 1, 1));
    }
    if (ellipse) {
        const std::int32_t commonOffset = random.nextInt(std::max(ellipseA - 5, 1));
        xOffset = commonOffset;
        zOffset = commonOffset;
    }
    const DecorationPos localOrigin{.x = signX * xOffset, .y = 0, .z = signZ * zOffset};
    const double angle = ellipse ? shapeAngle + std::numbers::pi / 2 : random.nextDouble() * 2.0 * std::numbers::pi;

    for (std::int32_t y = 0; y < height - 3; ++y) {
        const std::int32_t radius = heightDependentRadiusRound(random, y, height, width);
        carve(radius, y, globalOrigin, region, false, angle, localOrigin, ellipseA, ellipseC);
    }
    for (std::int32_t y = -1; y > -height + random.nextInt(5); --y) {
        const std::int32_t radius = heightDependentRadiusSteep(random, -y, height, width);
        carve(radius, y, globalOrigin, region, true, angle, localOrigin, ellipseA, ellipseC);
    }
}

} // namespace

void icebergFeature::place(ChunkFeatureRegion& region, random::RandomSource& random,
                           const DecorationPos& suppliedOrigin, const std::int32_t mainStateId) {
    const DecorationPos origin{.x = suppliedOrigin.x, .y = kSeaLevel, .z = suppliedOrigin.z};
    const bool snowOnTop = random.nextDouble() > 0.7;
    const double shapeAngle = random.nextDouble() * 2.0 * std::numbers::pi;
    const std::int32_t ellipseA = 11 - random.nextInt(5);
    const std::int32_t ellipseC = 3 + random.nextInt(3);
    const bool ellipse = random.nextDouble() > 0.7;
    std::int32_t aboveWaterHeight = ellipse ? random.nextInt(6) + 6 : random.nextInt(15) + 3;
    if (!ellipse && random.nextDouble() > 0.9) {
        aboveWaterHeight += random.nextInt(19) + 7;
    }
    const std::int32_t belowWaterHeight = std::min(aboveWaterHeight + random.nextInt(11), 18);
    const std::int32_t width = std::min(aboveWaterHeight + random.nextInt(7) - random.nextInt(5), 11);
    const std::int32_t a = ellipse ? ellipseA : 11;

    for (std::int32_t xOffset = -a; xOffset < a; ++xOffset) {
        for (std::int32_t zOffset = -a; zOffset < a; ++zOffset) {
            for (std::int32_t yOffset = 0; yOffset < aboveWaterHeight; ++yOffset) {
                const std::int32_t radius = ellipse
                                                ? heightDependentRadiusEllipse(yOffset, aboveWaterHeight, width)
                                                : heightDependentRadiusRound(random, yOffset, aboveWaterHeight, width);
                if (ellipse || xOffset < radius) {
                    generateBlock(region, random, origin, aboveWaterHeight, xOffset, yOffset, zOffset, radius, a,
                                  ellipse, ellipseC, shapeAngle, snowOnTop, mainStateId);
                }
            }
        }
    }

    smooth(region, origin, width, aboveWaterHeight, ellipse, ellipseA);

    for (std::int32_t xOffset = -a; xOffset < a; ++xOffset) {
        for (std::int32_t zOffset = -a; zOffset < a; ++zOffset) {
            for (std::int32_t yOffset = -1; yOffset > -belowWaterHeight; --yOffset) {
                const std::int32_t newA =
                    ellipse ? wire::common::toIntLikeJava(static_cast<double>(std::ceil(
                                  static_cast<float>(a) * (1.0F - squared(static_cast<float>(yOffset)) /
                                                                      (static_cast<float>(belowWaterHeight) * 8.0F)))))
                            : a;
                const std::int32_t radius = heightDependentRadiusSteep(random, -yOffset, belowWaterHeight, width);
                if (xOffset < radius) {
                    generateBlock(region, random, origin, belowWaterHeight, xOffset, yOffset, zOffset, radius, newA,
                                  ellipse, ellipseC, shapeAngle, snowOnTop, mainStateId);
                }
            }
        }
    }

    const bool cutOut = ellipse ? random.nextDouble() > 0.1 : random.nextDouble() > 0.7;
    if (cutOut) {
        generateCutOut(random, region, width, aboveWaterHeight, origin, ellipse, ellipseA, shapeAngle, ellipseC);
    }
}

} // namespace basalt::server::world::terrain::decoration::frozenocean
