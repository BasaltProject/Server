package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.spline.BoundedFloatFunction
import org.basaltmc.server.world.spline.CubicSpline
import org.basaltmc.server.world.spline.CubicSplineBuilder
import org.basaltmc.wire.common.Mth
import kotlin.math.abs

/**
 * // parity: net.minecraft.data.worldgen.TerrainProvider, the Overworld offset, factor and
 * jaggedness splines. The control points are hardcoded in that class in the jar, not datapack
 * JSON, the same category of material as [BlendedNoise].
 *
 * Only the non-amplified path is ported: the Amplified preset's alternate value transformers are
 * a documented gap, so the flag vanilla threads through every method is dropped rather than
 * carried unused.
 */
public object TerrainProvider {
    public fun <C, I : BoundedFloatFunction<C>> overworldOffset(
        continents: I,
        erosion: I,
        ridges: I,
    ): CubicSpline<C, I> {
        val beachSpline =
            buildErosionOffsetSpline(
                erosion,
                ridges,
                -0.15f,
                0.0f,
                0.0f,
                0.1f,
                0.0f,
                -0.03f,
                includeExtremeHills = false,
                saddle = false,
            )
        val lowSpline =
            buildErosionOffsetSpline(
                erosion,
                ridges,
                -0.1f,
                0.03f,
                0.1f,
                0.1f,
                0.01f,
                -0.03f,
                includeExtremeHills = false,
                saddle = false,
            )
        val midSpline =
            buildErosionOffsetSpline(
                erosion,
                ridges,
                -0.1f,
                0.03f,
                0.1f,
                0.7f,
                0.01f,
                -0.03f,
                includeExtremeHills = true,
                saddle = true,
            )
        val highSpline =
            buildErosionOffsetSpline(
                erosion,
                ridges,
                -0.05f,
                0.03f,
                0.1f,
                1.0f,
                0.01f,
                0.01f,
                includeExtremeHills = true,
                saddle = true,
            )
        return CubicSplineBuilder<C, I>(continents)
            .addPoint(-1.1f, 0.044f)
            .addPoint(-1.02f, -0.2222f)
            .addPoint(-0.51f, -0.2222f)
            .addPoint(-0.44f, -0.12f)
            .addPoint(-0.18f, -0.12f)
            .addPoint(-0.16f, beachSpline)
            .addPoint(-0.15f, beachSpline)
            .addPoint(-0.1f, lowSpline)
            .addPoint(0.25f, midSpline)
            .addPoint(1.0f, highSpline)
            .build()
    }

    public fun <C, I : BoundedFloatFunction<C>> overworldFactor(
        continents: I,
        erosion: I,
        weirdness: I,
        ridges: I,
    ): CubicSpline<C, I> =
        CubicSplineBuilder<C, I>(continents)
            .addPoint(-0.19f, 3.95f)
            .addPoint(-0.15f, getErosionFactor(erosion, weirdness, ridges, 6.25f, shatteredTerrain = true))
            .addPoint(-0.1f, getErosionFactor(erosion, weirdness, ridges, 5.47f, shatteredTerrain = true))
            .addPoint(0.03f, getErosionFactor(erosion, weirdness, ridges, 5.08f, shatteredTerrain = true))
            .addPoint(0.06f, getErosionFactor(erosion, weirdness, ridges, 4.69f, shatteredTerrain = false))
            .build()

    public fun <C, I : BoundedFloatFunction<C>> overworldJaggedness(
        continents: I,
        erosion: I,
        weirdness: I,
        ridges: I,
    ): CubicSpline<C, I> =
        CubicSplineBuilder<C, I>(continents)
            .addPoint(-0.11f, 0.0f)
            .addPoint(0.03f, buildErosionJaggednessSpline(erosion, weirdness, ridges, 1.0f, 0.5f, 0.0f, 0.0f))
            .addPoint(0.65f, buildErosionJaggednessSpline(erosion, weirdness, ridges, 1.0f, 1.0f, 1.0f, 0.0f))
            .build()

    private fun <C, I : BoundedFloatFunction<C>> buildErosionJaggednessSpline(
        erosion: I,
        weirdness: I,
        ridges: I,
        jaggednessFactorAtPeakRidgeAndErosionIndex0: Float,
        jaggednessFactorAtPeakRidgeAndErosionIndex1: Float,
        jaggednessFactorAtHighRidgeAndErosionIndex0: Float,
        jaggednessFactorAtHighRidgeAndErosionIndex1: Float,
    ): CubicSpline<C, I> {
        val ridgeJaggednessSplineAtErosion0 =
            buildRidgeJaggednessSpline(
                weirdness,
                ridges,
                jaggednessFactorAtPeakRidgeAndErosionIndex0,
                jaggednessFactorAtHighRidgeAndErosionIndex0,
            )
        val ridgeJaggednessSplineAtErosion1 =
            buildRidgeJaggednessSpline(
                weirdness,
                ridges,
                jaggednessFactorAtPeakRidgeAndErosionIndex1,
                jaggednessFactorAtHighRidgeAndErosionIndex1,
            )
        return CubicSplineBuilder<C, I>(erosion)
            .addPoint(-1.0f, ridgeJaggednessSplineAtErosion0)
            .addPoint(-0.78f, ridgeJaggednessSplineAtErosion1)
            .addPoint(-0.5775f, ridgeJaggednessSplineAtErosion1)
            .addPoint(-0.375f, 0.0f)
            .build()
    }

    /** // parity: net.minecraft.data.worldgen.TerrainProvider.peaksAndValleys(float). */
    public fun peaksAndValleys(weirdness: Float): Float = -(abs(abs(weirdness) - 0.6666667f) - 0.33333334f) * 3.0f

    private fun <C, I : BoundedFloatFunction<C>> buildRidgeJaggednessSpline(
        weirdness: I,
        ridges: I,
        jaggednessFactorAtPeakRidge: Float,
        jaggednessFactorAtHighRidge: Float,
    ): CubicSpline<C, I> {
        val highSliceStart = peaksAndValleys(0.4f)
        val highSliceEnd = peaksAndValleys(0.56666666f)
        val highSliceMiddle = (highSliceStart + highSliceEnd) / 2.0f
        val ridgeSpline = CubicSplineBuilder<C, I>(ridges)
        ridgeSpline.addPoint(highSliceStart, 0.0f)
        if (jaggednessFactorAtHighRidge > 0.0f) {
            ridgeSpline.addPoint(
                highSliceMiddle,
                buildWeirdnessJaggednessSpline(weirdness, jaggednessFactorAtHighRidge),
            )
        } else {
            ridgeSpline.addPoint(highSliceMiddle, 0.0f)
        }
        if (jaggednessFactorAtPeakRidge > 0.0f) {
            ridgeSpline.addPoint(1.0f, buildWeirdnessJaggednessSpline(weirdness, jaggednessFactorAtPeakRidge))
        } else {
            ridgeSpline.addPoint(1.0f, 0.0f)
        }
        return ridgeSpline.build()
    }

    private fun <C, I : BoundedFloatFunction<C>> buildWeirdnessJaggednessSpline(
        weirdness: I,
        jaggednessFactor: Float,
    ): CubicSpline<C, I> {
        val maxJaggednessAtNegativeWeirdness = 0.63f * jaggednessFactor
        val maxJaggednessAtPositiveWeirdness = 0.3f * jaggednessFactor
        return CubicSplineBuilder<C, I>(weirdness)
            .addPoint(-0.01f, maxJaggednessAtNegativeWeirdness)
            .addPoint(0.01f, maxJaggednessAtPositiveWeirdness)
            .build()
    }

    private fun <C, I : BoundedFloatFunction<C>> getErosionFactor(
        erosion: I,
        weirdness: I,
        ridges: I,
        baseValue: Float,
        shatteredTerrain: Boolean,
    ): CubicSpline<C, I> {
        val baseSpline = CubicSplineBuilder<C, I>(weirdness).addPoint(-0.2f, 6.3f).addPoint(0.2f, baseValue).build()
        val erosionPoints =
            CubicSplineBuilder<C, I>(erosion)
                .addPoint(-0.6f, baseSpline)
                .addPoint(
                    -0.5f,
                    CubicSplineBuilder<C, I>(weirdness).addPoint(-0.05f, 6.3f).addPoint(0.05f, 2.67f).build(),
                ).addPoint(-0.35f, baseSpline)
                .addPoint(-0.25f, baseSpline)
                .addPoint(
                    -0.1f,
                    CubicSplineBuilder<C, I>(weirdness).addPoint(-0.05f, 2.67f).addPoint(0.05f, 6.3f).build(),
                ).addPoint(0.03f, baseSpline)
        if (shatteredTerrain) {
            val weirdnessShattered =
                CubicSplineBuilder<C, I>(
                    weirdness,
                ).addPoint(0.0f, baseValue).addPoint(0.1f, 0.625f).build()
            val ridgesShattered =
                CubicSplineBuilder<C, I>(ridges)
                    .addPoint(-0.9f, baseValue)
                    .addPoint(-0.69f, weirdnessShattered)
                    .build()
            erosionPoints
                .addPoint(
                    0.35f,
                    baseValue,
                ).addPoint(0.45f, ridgesShattered)
                .addPoint(0.55f, ridgesShattered)
                .addPoint(0.62f, baseValue)
        } else {
            val extremeHillsTerrainFromMidSliceAndUp =
                CubicSplineBuilder<C, I>(ridges)
                    .addPoint(-0.7f, baseSpline)
                    .addPoint(-0.15f, 1.37f)
                    .build()
            val extra3dNoiseOnPeaksOnly =
                CubicSplineBuilder<C, I>(ridges).addPoint(0.45f, baseSpline).addPoint(0.7f, 1.56f).build()
            erosionPoints
                .addPoint(0.05f, extra3dNoiseOnPeaksOnly)
                .addPoint(0.4f, extra3dNoiseOnPeaksOnly)
                .addPoint(0.45f, extremeHillsTerrainFromMidSliceAndUp)
                .addPoint(0.55f, extremeHillsTerrainFromMidSliceAndUp)
                .addPoint(0.58f, baseValue)
        }
        return erosionPoints.build()
    }

    private fun calculateSlope(
        y1: Float,
        y2: Float,
        x1: Float,
        x2: Float,
    ): Float = (y2 - y1) / (x2 - x1)

    private fun <C, I : BoundedFloatFunction<C>> buildMountainRidgeSplineWithPoints(
        ridges: I,
        modulation: Float,
        saddle: Boolean,
    ): CubicSpline<C, I> {
        val build = CubicSplineBuilder<C, I>(ridges)
        val minPointContinentalness = mountainContinentalness(-1.0f, modulation, -0.7f)
        val maxPointContinentalness = mountainContinentalness(1.0f, modulation, -0.7f)
        val ridgeZeroPoint = calculateMountainRidgeZeroContinentalnessPoint(modulation)
        if (-0.65f < ridgeZeroPoint && ridgeZeroPoint < 1.0f) {
            val afterRiverThresholdContinentalness = mountainContinentalness(-0.65f, modulation, -0.7f)
            val beforeRiverThresholdContinentalness = mountainContinentalness(-0.75f, modulation, -0.7f)
            val minPointDerivative =
                calculateSlope(minPointContinentalness, beforeRiverThresholdContinentalness, -1.0f, -0.75f)
            build.addPoint(-1.0f, minPointContinentalness, minPointDerivative)
            build.addPoint(-0.75f, beforeRiverThresholdContinentalness)
            build.addPoint(-0.65f, afterRiverThresholdContinentalness)
            val ridgeZeroPointContinentalness = mountainContinentalness(ridgeZeroPoint, modulation, -0.7f)
            val maxPointDerivative =
                calculateSlope(ridgeZeroPointContinentalness, maxPointContinentalness, ridgeZeroPoint, 1.0f)
            build.addPoint(ridgeZeroPoint - 0.01f, ridgeZeroPointContinentalness)
            build.addPoint(ridgeZeroPoint, ridgeZeroPointContinentalness, maxPointDerivative)
            build.addPoint(1.0f, maxPointContinentalness, maxPointDerivative)
        } else {
            val simpleDerivative = calculateSlope(minPointContinentalness, maxPointContinentalness, -1.0f, 1.0f)
            if (saddle) {
                build.addPoint(-1.0f, maxOf(0.2f, minPointContinentalness))
                build.addPoint(0.0f, Mth.lerp(0.5f, minPointContinentalness, maxPointContinentalness), simpleDerivative)
            } else {
                build.addPoint(-1.0f, minPointContinentalness, simpleDerivative)
            }
            build.addPoint(1.0f, maxPointContinentalness, simpleDerivative)
        }
        return build.build()
    }

    private fun mountainContinentalness(
        ridge: Float,
        modulation: Float,
        allowRiversBelow: Float,
    ): Float {
        val ridgeSlope = 1.0f - (1.0f - modulation) * 0.5f
        val ridgeIntersect = 0.5f * (1.0f - modulation)
        val adjustedRidgeHeight = (ridge + 1.17f) * 0.46082947f
        val continentalness = adjustedRidgeHeight * ridgeSlope - ridgeIntersect
        return if (ridge < allowRiversBelow) maxOf(continentalness, -0.2222f) else maxOf(continentalness, 0.0f)
    }

    private fun calculateMountainRidgeZeroContinentalnessPoint(modulation: Float): Float {
        val ridgeSlope = 1.0f - (1.0f - modulation) * 0.5f
        val ridgeIntersect = 0.5f * (1.0f - modulation)
        return ridgeIntersect / (0.46082947f * ridgeSlope) - 1.17f
    }

    public fun <C, I : BoundedFloatFunction<C>> buildErosionOffsetSpline(
        erosion: I,
        ridges: I,
        lowValley: Float,
        hill: Float,
        tallHill: Float,
        mountainFactor: Float,
        plain: Float,
        swamp: Float,
        includeExtremeHills: Boolean,
        saddle: Boolean,
    ): CubicSpline<C, I> {
        val veryLowErosionMountains =
            buildMountainRidgeSplineWithPoints(ridges, Mth.lerp(mountainFactor, 0.6f, 1.5f), saddle)
        val lowErosionMountains =
            buildMountainRidgeSplineWithPoints(ridges, Mth.lerp(mountainFactor, 0.6f, 1.0f), saddle)
        val mountains = buildMountainRidgeSplineWithPoints(ridges, mountainFactor, saddle)
        val widePlateau =
            ridgeSpline(
                ridges,
                lowValley - 0.15f,
                0.5f * mountainFactor,
                Mth.lerp(0.5f, 0.5f, 0.5f) * mountainFactor,
                0.5f * mountainFactor,
                0.6f * mountainFactor,
                0.5f,
            )
        val narrowPlateau =
            ridgeSpline(
                ridges,
                lowValley,
                plain * mountainFactor,
                hill * mountainFactor,
                0.5f * mountainFactor,
                0.6f * mountainFactor,
                0.5f,
            )
        val plains = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5f)
        val plainsFarInland = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5f)
        val extremeHills =
            CubicSplineBuilder<C, I>(ridges)
                .addPoint(-1.0f, lowValley)
                .addPoint(-0.4f, plains)
                .addPoint(0.0f, tallHill + 0.07f)
                .build()
        val swamps = ridgeSpline(ridges, -0.02f, swamp, swamp, hill, tallHill, 0.0f)
        val builder =
            CubicSplineBuilder<C, I>(erosion)
                .addPoint(-0.85f, veryLowErosionMountains)
                .addPoint(-0.7f, lowErosionMountains)
                .addPoint(-0.4f, mountains)
                .addPoint(-0.35f, widePlateau)
                .addPoint(-0.1f, narrowPlateau)
                .addPoint(0.2f, plains)
        if (includeExtremeHills) {
            builder
                .addPoint(
                    0.4f,
                    plainsFarInland,
                ).addPoint(0.45f, extremeHills)
                .addPoint(0.55f, extremeHills)
                .addPoint(0.58f, plainsFarInland)
        }
        builder.addPoint(0.7f, swamps)
        return builder.build()
    }

    private fun <C, I : BoundedFloatFunction<C>> ridgeSpline(
        ridges: I,
        valley: Float,
        low: Float,
        mid: Float,
        high: Float,
        peaks: Float,
        minValleySteepness: Float,
    ): CubicSpline<C, I> {
        val d1 = maxOf(0.5f * (low - valley), minValleySteepness)
        val d2 = 5.0f * (mid - low)
        return CubicSplineBuilder<C, I>(ridges)
            .addPoint(-1.0f, valley, d1)
            .addPoint(-0.4f, low, minOf(d1, d2))
            .addPoint(0.0f, mid, d2)
            .addPoint(0.4f, high, 2.0f * (high - mid))
            .addPoint(1.0f, peaks, 0.7f * (peaks - high))
            .build()
    }
}
