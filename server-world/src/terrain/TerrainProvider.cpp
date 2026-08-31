#include <basalt/server/world/terrain/TerrainProvider.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::world::terrain::terrainProvider {
namespace {

namespace mth = wire::common::mth;
using Builder = spline::CubicSplineBuilder<densityfunction::FunctionContext>;

[[nodiscard]] float calculateSlope(float y1, float y2, float x1, float x2) { return (y2 - y1) / (x2 - x1); }

[[nodiscard]] float mountainContinentalness(float ridge, float modulation, float allowRiversBelow) {
    const float ridgeSlope = 1.0F - (1.0F - modulation) * 0.5F;
    const float ridgeIntersect = 0.5F * (1.0F - modulation);
    const float adjustedRidgeHeight = (ridge + 1.17F) * 0.46082947F;
    const float continentalness = adjustedRidgeHeight * ridgeSlope - ridgeIntersect;
    return ridge < allowRiversBelow ? std::max(continentalness, -0.2222F) : std::max(continentalness, 0.0F);
}

[[nodiscard]] float calculateMountainRidgeZeroContinentalnessPoint(float modulation) {
    const float ridgeSlope = 1.0F - (1.0F - modulation) * 0.5F;
    const float ridgeIntersect = 0.5F * (1.0F - modulation);
    return ridgeIntersect / (0.46082947F * ridgeSlope) - 1.17F;
}

[[nodiscard]] TerrainSpline buildMountainRidgeSplineWithPoints(const Coordinate& ridges, float modulation,
                                                               bool saddle) {
    Builder build{ridges};
    const float minPointContinentalness = mountainContinentalness(-1.0F, modulation, -0.7F);
    const float maxPointContinentalness = mountainContinentalness(1.0F, modulation, -0.7F);
    const float ridgeZeroPoint = calculateMountainRidgeZeroContinentalnessPoint(modulation);
    if (-0.65F < ridgeZeroPoint && ridgeZeroPoint < 1.0F) {
        const float afterRiverThreshold = mountainContinentalness(-0.65F, modulation, -0.7F);
        const float beforeRiverThreshold = mountainContinentalness(-0.75F, modulation, -0.7F);
        const float minPointDerivative = calculateSlope(minPointContinentalness, beforeRiverThreshold, -1.0F, -0.75F);
        build.addPoint(-1.0F, minPointContinentalness, minPointDerivative);
        build.addPoint(-0.75F, beforeRiverThreshold);
        build.addPoint(-0.65F, afterRiverThreshold);
        const float ridgeZeroPointContinentalness = mountainContinentalness(ridgeZeroPoint, modulation, -0.7F);
        const float maxPointDerivative =
            calculateSlope(ridgeZeroPointContinentalness, maxPointContinentalness, ridgeZeroPoint, 1.0F);
        build.addPoint(ridgeZeroPoint - 0.01F, ridgeZeroPointContinentalness);
        build.addPoint(ridgeZeroPoint, ridgeZeroPointContinentalness, maxPointDerivative);
        build.addPoint(1.0F, maxPointContinentalness, maxPointDerivative);
    } else {
        const float simpleDerivative = calculateSlope(minPointContinentalness, maxPointContinentalness, -1.0F, 1.0F);
        if (saddle) {
            build.addPoint(-1.0F, std::max(0.2F, minPointContinentalness));
            build.addPoint(0.0F, mth::lerp(0.5F, minPointContinentalness, maxPointContinentalness), simpleDerivative);
        } else {
            build.addPoint(-1.0F, minPointContinentalness, simpleDerivative);
        }
        build.addPoint(1.0F, maxPointContinentalness, simpleDerivative);
    }
    return build.build();
}

[[nodiscard]] TerrainSpline ridgeSpline(const Coordinate& ridges, float valley, float low, float mid, float high,
                                        float peaks, float minValleySteepness) {
    const float d1 = std::max(0.5F * (low - valley), minValleySteepness);
    const float d2 = 5.0F * (mid - low);
    return Builder{ridges}
        .addPoint(-1.0F, valley, d1)
        .addPoint(-0.4F, low, std::min(d1, d2))
        .addPoint(0.0F, mid, d2)
        .addPoint(0.4F, high, 2.0F * (high - mid))
        .addPoint(1.0F, peaks, 0.7F * (peaks - high))
        .build();
}

[[nodiscard]] TerrainSpline buildWeirdnessJaggednessSpline(const Coordinate& weirdness, float jaggednessFactor) {
    const float maxAtNegative = 0.63F * jaggednessFactor;
    const float maxAtPositive = 0.3F * jaggednessFactor;
    return Builder{weirdness}.addPoint(-0.01F, maxAtNegative).addPoint(0.01F, maxAtPositive).build();
}

[[nodiscard]] TerrainSpline buildRidgeJaggednessSpline(const Coordinate& weirdness, const Coordinate& ridges,
                                                       float atPeakRidge, float atHighRidge) {
    const float highSliceStart = peaksAndValleys(0.4F);
    const float highSliceEnd = peaksAndValleys(0.56666666F);
    const float highSliceMiddle = (highSliceStart + highSliceEnd) / 2.0F;
    Builder ridgeSplineBuilder{ridges};
    ridgeSplineBuilder.addPoint(highSliceStart, 0.0F);
    if (atHighRidge > 0.0F) {
        ridgeSplineBuilder.addPoint(highSliceMiddle, buildWeirdnessJaggednessSpline(weirdness, atHighRidge));
    } else {
        ridgeSplineBuilder.addPoint(highSliceMiddle, 0.0F);
    }
    if (atPeakRidge > 0.0F) {
        ridgeSplineBuilder.addPoint(1.0F, buildWeirdnessJaggednessSpline(weirdness, atPeakRidge));
    } else {
        ridgeSplineBuilder.addPoint(1.0F, 0.0F);
    }
    return ridgeSplineBuilder.build();
}

[[nodiscard]] TerrainSpline buildErosionJaggednessSpline(const Coordinate& erosion, const Coordinate& weirdness,
                                                         const Coordinate& ridges, float atPeakErosion0,
                                                         float atPeakErosion1, float atHighErosion0,
                                                         float atHighErosion1) {
    const TerrainSpline atErosion0 = buildRidgeJaggednessSpline(weirdness, ridges, atPeakErosion0, atHighErosion0);
    const TerrainSpline atErosion1 = buildRidgeJaggednessSpline(weirdness, ridges, atPeakErosion1, atHighErosion1);
    return Builder{erosion}
        .addPoint(-1.0F, atErosion0)
        .addPoint(-0.78F, atErosion1)
        .addPoint(-0.5775F, atErosion1)
        .addPoint(-0.375F, 0.0F)
        .build();
}

[[nodiscard]] TerrainSpline getErosionFactor(const Coordinate& erosion, const Coordinate& weirdness,
                                             const Coordinate& ridges, float baseValue, bool shatteredTerrain) {
    const TerrainSpline baseSpline = Builder{weirdness}.addPoint(-0.2F, 6.3F).addPoint(0.2F, baseValue).build();
    Builder erosionPoints{erosion};
    erosionPoints.addPoint(-0.6F, baseSpline)
        .addPoint(-0.5F, Builder{weirdness}.addPoint(-0.05F, 6.3F).addPoint(0.05F, 2.67F).build())
        .addPoint(-0.35F, baseSpline)
        .addPoint(-0.25F, baseSpline)
        .addPoint(-0.1F, Builder{weirdness}.addPoint(-0.05F, 2.67F).addPoint(0.05F, 6.3F).build())
        .addPoint(0.03F, baseSpline);
    if (shatteredTerrain) {
        const TerrainSpline weirdnessShattered =
            Builder{weirdness}.addPoint(0.0F, baseValue).addPoint(0.1F, 0.625F).build();
        const TerrainSpline ridgesShattered =
            Builder{ridges}.addPoint(-0.9F, baseValue).addPoint(-0.69F, weirdnessShattered).build();
        erosionPoints.addPoint(0.35F, baseValue)
            .addPoint(0.45F, ridgesShattered)
            .addPoint(0.55F, ridgesShattered)
            .addPoint(0.62F, baseValue);
    } else {
        const TerrainSpline extremeHillsFromMidSliceAndUp =
            Builder{ridges}.addPoint(-0.7F, baseSpline).addPoint(-0.15F, 1.37F).build();
        const TerrainSpline extra3dNoiseOnPeaksOnly =
            Builder{ridges}.addPoint(0.45F, baseSpline).addPoint(0.7F, 1.56F).build();
        erosionPoints.addPoint(0.05F, extra3dNoiseOnPeaksOnly)
            .addPoint(0.4F, extra3dNoiseOnPeaksOnly)
            .addPoint(0.45F, extremeHillsFromMidSliceAndUp)
            .addPoint(0.55F, extremeHillsFromMidSliceAndUp)
            .addPoint(0.58F, baseValue);
    }
    return erosionPoints.build();
}

} // namespace

float peaksAndValleys(float weirdness) { return -(std::abs(std::abs(weirdness) - 0.6666667F) - 0.33333334F) * 3.0F; }

TerrainSpline buildErosionOffsetSpline(const Coordinate& erosion, const Coordinate& ridges, float lowValley, float hill,
                                       float tallHill, float mountainFactor, float plain, float swamp,
                                       bool includeExtremeHills, bool saddle) {
    const TerrainSpline veryLowErosionMountains =
        buildMountainRidgeSplineWithPoints(ridges, mth::lerp(mountainFactor, 0.6F, 1.5F), saddle);
    const TerrainSpline lowErosionMountains =
        buildMountainRidgeSplineWithPoints(ridges, mth::lerp(mountainFactor, 0.6F, 1.0F), saddle);
    const TerrainSpline mountains = buildMountainRidgeSplineWithPoints(ridges, mountainFactor, saddle);
    const TerrainSpline widePlateau =
        ridgeSpline(ridges, lowValley - 0.15F, 0.5F * mountainFactor, mth::lerp(0.5F, 0.5F, 0.5F) * mountainFactor,
                    0.5F * mountainFactor, 0.6F * mountainFactor, 0.5F);
    const TerrainSpline narrowPlateau = ridgeSpline(ridges, lowValley, plain * mountainFactor, hill * mountainFactor,
                                                    0.5F * mountainFactor, 0.6F * mountainFactor, 0.5F);
    const TerrainSpline plains = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5F);
    const TerrainSpline plainsFarInland = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5F);
    const TerrainSpline extremeHills =
        Builder{ridges}.addPoint(-1.0F, lowValley).addPoint(-0.4F, plains).addPoint(0.0F, tallHill + 0.07F).build();
    const TerrainSpline swamps = ridgeSpline(ridges, -0.02F, swamp, swamp, hill, tallHill, 0.0F);

    Builder builder{erosion};
    builder.addPoint(-0.85F, veryLowErosionMountains)
        .addPoint(-0.7F, lowErosionMountains)
        .addPoint(-0.4F, mountains)
        .addPoint(-0.35F, widePlateau)
        .addPoint(-0.1F, narrowPlateau)
        .addPoint(0.2F, plains);
    if (includeExtremeHills) {
        builder.addPoint(0.4F, plainsFarInland)
            .addPoint(0.45F, extremeHills)
            .addPoint(0.55F, extremeHills)
            .addPoint(0.58F, plainsFarInland);
    }
    builder.addPoint(0.7F, swamps);
    return builder.build();
}

TerrainSpline overworldOffset(const Coordinate& continents, const Coordinate& erosion, const Coordinate& ridges) {
    const TerrainSpline beachSpline =
        buildErosionOffsetSpline(erosion, ridges, -0.15F, 0.0F, 0.0F, 0.1F, 0.0F, -0.03F, false, false);
    const TerrainSpline lowSpline =
        buildErosionOffsetSpline(erosion, ridges, -0.1F, 0.03F, 0.1F, 0.1F, 0.01F, -0.03F, false, false);
    const TerrainSpline midSpline =
        buildErosionOffsetSpline(erosion, ridges, -0.1F, 0.03F, 0.1F, 0.7F, 0.01F, -0.03F, true, true);
    const TerrainSpline highSpline =
        buildErosionOffsetSpline(erosion, ridges, -0.05F, 0.03F, 0.1F, 1.0F, 0.01F, 0.01F, true, true);
    return Builder{continents}
        .addPoint(-1.1F, 0.044F)
        .addPoint(-1.02F, -0.2222F)
        .addPoint(-0.51F, -0.2222F)
        .addPoint(-0.44F, -0.12F)
        .addPoint(-0.18F, -0.12F)
        // The same spline at two points, which is why a child is shared and not owned outright.
        .addPoint(-0.16F, beachSpline)
        .addPoint(-0.15F, beachSpline)
        .addPoint(-0.1F, lowSpline)
        .addPoint(0.25F, midSpline)
        .addPoint(1.0F, highSpline)
        .build();
}

TerrainSpline overworldFactor(const Coordinate& continents, const Coordinate& erosion, const Coordinate& weirdness,
                              const Coordinate& ridges) {
    return Builder{continents}
        .addPoint(-0.19F, 3.95F)
        .addPoint(-0.15F, getErosionFactor(erosion, weirdness, ridges, 6.25F, true))
        .addPoint(-0.1F, getErosionFactor(erosion, weirdness, ridges, 5.47F, true))
        .addPoint(0.03F, getErosionFactor(erosion, weirdness, ridges, 5.08F, true))
        .addPoint(0.06F, getErosionFactor(erosion, weirdness, ridges, 4.69F, false))
        .build();
}

TerrainSpline overworldJaggedness(const Coordinate& continents, const Coordinate& erosion, const Coordinate& weirdness,
                                  const Coordinate& ridges) {
    return Builder{continents}
        .addPoint(-0.11F, 0.0F)
        .addPoint(0.03F, buildErosionJaggednessSpline(erosion, weirdness, ridges, 1.0F, 0.5F, 0.0F, 0.0F))
        .addPoint(0.65F, buildErosionJaggednessSpline(erosion, weirdness, ridges, 1.0F, 1.0F, 1.0F, 0.0F))
        .build();
}

} // namespace basalt::server::world::terrain::terrainProvider
