// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;

import it.unimi.dsi.fastutil.floats.Float2FloatFunction;
import net.minecraft.util.BoundedFloatFunction;
import net.minecraft.util.CubicSpline;
import net.minecraft.util.Mth;

public class TerrainProvider {
    private static final Float2FloatFunction kIdentity = Float2FloatFunction.identity();
    private static final Float2FloatFunction kAmplifiedOffset = v -> v < 0.0F ? v : v * 2.0F;
    private static final Float2FloatFunction kAmplifiedFactor = v -> 1.25F - 6.25F / (v + 5.0F);
    private static final Float2FloatFunction kAmplifiedJaggedness = v -> v * 2.0F;

    public static <I extends BoundedFloatFunction<?>> CubicSpline<I> overworldOffset(I continents, I erosion, I ridges, boolean amplified) {
        Float2FloatFunction t = amplified ? kAmplifiedOffset : kIdentity;
        CubicSpline<I> beach = buildErosionOffsetSpline(erosion, ridges, -0.15F, 0.0F, 0.0F, 0.1F, 0.0F, -0.03F, false, false, t);
        CubicSpline<I> low = buildErosionOffsetSpline(erosion, ridges, -0.1F, 0.03F, 0.1F, 0.1F, 0.01F, -0.03F, false, false, t);
        CubicSpline<I> mid = buildErosionOffsetSpline(erosion, ridges, -0.1F, 0.03F, 0.1F, 0.7F, 0.01F, -0.03F, true, true, t);
        CubicSpline<I> high = buildErosionOffsetSpline(erosion, ridges, -0.05F, 0.03F, 0.1F, 1.0F, 0.01F, 0.01F, true, true, t);
        return CubicSpline.<I>builder(continents, t)
            .addPoint(-1.1F, 0.044F).addPoint(-1.02F, -0.2222F).addPoint(-0.51F, -0.2222F)
            .addPoint(-0.44F, -0.12F).addPoint(-0.18F, -0.12F)
            .addPoint(-0.16F, beach).addPoint(-0.15F, beach)
            .addPoint(-0.1F, low).addPoint(0.25F, mid).addPoint(1.0F, high).build();
    }

    public static <I extends BoundedFloatFunction<?>> CubicSpline<I> overworldFactor(I continents, I erosion, I weirdness, I ridges, boolean amplified) {
        Float2FloatFunction t = amplified ? kAmplifiedFactor : kIdentity;
        return CubicSpline.<I>builder(continents, kIdentity)
            .addPoint(-0.19F, 3.95F)
            .addPoint(-0.15F, erosionFactor(erosion, weirdness, ridges, 6.25F, true, kIdentity))
            .addPoint(-0.1F, erosionFactor(erosion, weirdness, ridges, 5.47F, true, t))
            .addPoint(0.03F, erosionFactor(erosion, weirdness, ridges, 5.08F, true, t))
            .addPoint(0.06F, erosionFactor(erosion, weirdness, ridges, 4.69F, false, t)).build();
    }

    public static <I extends BoundedFloatFunction<?>> CubicSpline<I> overworldJaggedness(I continents, I erosion, I weirdness, I ridges, boolean amplified) {
        Float2FloatFunction t = amplified ? kAmplifiedJaggedness : kIdentity;
        return CubicSpline.<I>builder(continents, t)
            .addPoint(-0.11F, 0.0F)
            .addPoint(0.03F, erosionJaggedness(erosion, weirdness, ridges, 1.0F, 0.5F, 0.0F, 0.0F, t))
            .addPoint(0.65F, erosionJaggedness(erosion, weirdness, ridges, 1.0F, 1.0F, 1.0F, 0.0F, t)).build();
    }

    public static float peaksAndValleys(float weirdness) {
        return -(Math.abs(Math.abs(weirdness) - 0.6666667F) - 0.33333334F) * 3.0F;
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> erosionJaggedness(I erosion, I weirdness, I ridges, float peak0, float peak1, float high0, float high1, Float2FloatFunction t) {
        CubicSpline<I> atZero = ridgeJaggedness(weirdness, ridges, peak0, high0, t);
        CubicSpline<I> atOne = ridgeJaggedness(weirdness, ridges, peak1, high1, t);
        return CubicSpline.<I>builder(erosion, t)
            .addPoint(-1.0F, atZero).addPoint(-0.78F, atOne).addPoint(-0.5775F, atOne).addPoint(-0.375F, 0.0F).build();
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> ridgeJaggedness(I weirdness, I ridges, float peak, float high, Float2FloatFunction t) {
        float start = peaksAndValleys(0.4F);
        float end = peaksAndValleys(0.56666666F);
        float middle = (start + end) / 2.0F;
        CubicSpline.Builder<I> spline = CubicSpline.<I>builder(ridges, t);
        spline.addPoint(start, 0.0F);
        if (high > 0.0F) {
            spline.addPoint(middle, weirdnessJaggedness(weirdness, high, t));
        } else {
            spline.addPoint(middle, 0.0F);
        }
        if (peak > 0.0F) {
            spline.addPoint(1.0F, weirdnessJaggedness(weirdness, peak, t));
        } else {
            spline.addPoint(1.0F, 0.0F);
        }
        return spline.build();
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> weirdnessJaggedness(I weirdness, float factor, Float2FloatFunction t) {
        return CubicSpline.<I>builder(weirdness, t).addPoint(-0.01F, 0.63F * factor).addPoint(0.01F, 0.3F * factor).build();
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> erosionFactor(I erosion, I weirdness, I ridges, float base, boolean shattered, Float2FloatFunction t) {
        CubicSpline<I> baseSpline = CubicSpline.<I>builder(weirdness, t).addPoint(-0.2F, 6.3F).addPoint(0.2F, base).build();
        CubicSpline<I> narrowDown = CubicSpline.<I>builder(weirdness, t).addPoint(-0.05F, 6.3F).addPoint(0.05F, 2.67F).build();
        CubicSpline<I> narrowUp = CubicSpline.<I>builder(weirdness, t).addPoint(-0.05F, 2.67F).addPoint(0.05F, 6.3F).build();
        CubicSpline.Builder<I> points = CubicSpline.<I>builder(erosion, t)
            .addPoint(-0.6F, baseSpline).addPoint(-0.5F, narrowDown).addPoint(-0.35F, baseSpline)
            .addPoint(-0.25F, baseSpline).addPoint(-0.1F, narrowUp).addPoint(0.03F, baseSpline);
        if (shattered) {
            CubicSpline<I> weirdnessShattered = CubicSpline.<I>builder(weirdness, t).addPoint(0.0F, base).addPoint(0.1F, 0.625F).build();
            CubicSpline<I> ridgesShattered = CubicSpline.<I>builder(ridges, t).addPoint(-0.9F, base).addPoint(-0.69F, weirdnessShattered).build();
            points.addPoint(0.35F, base).addPoint(0.45F, ridgesShattered).addPoint(0.55F, ridgesShattered).addPoint(0.62F, base);
        } else {
            CubicSpline<I> hills = CubicSpline.<I>builder(ridges, t).addPoint(-0.7F, baseSpline).addPoint(-0.15F, 1.37F).build();
            CubicSpline<I> peaks = CubicSpline.<I>builder(ridges, t).addPoint(0.45F, baseSpline).addPoint(0.7F, 1.56F).build();
            points.addPoint(0.05F, peaks).addPoint(0.4F, peaks).addPoint(0.45F, hills).addPoint(0.55F, hills).addPoint(0.58F, base);
        }
        return points.build();
    }

    private static float slope(float y1, float y2, float x1, float x2) {
        return (y2 - y1) / (x2 - x1);
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> mountainRidges(I ridges, float modulation, boolean saddle, Float2FloatFunction t) {
        CubicSpline.Builder<I> spline = CubicSpline.<I>builder(ridges, t);
        float riverLimit = -0.7F;
        float atMin = mountainContinentalness(-1.0F, modulation, riverLimit);
        float atMax = mountainContinentalness(1.0F, modulation, riverLimit);
        float zero = ridgeZeroPoint(modulation);
        if (-0.65F < zero && zero < 1.0F) {
            float afterRiver = mountainContinentalness(-0.65F, modulation, riverLimit);
            float beforeRiver = mountainContinentalness(-0.75F, modulation, riverLimit);
            spline.addPoint(-1.0F, atMin, slope(atMin, beforeRiver, -1.0F, -0.75F));
            spline.addPoint(-0.75F, beforeRiver);
            spline.addPoint(-0.65F, afterRiver);
            float atZero = mountainContinentalness(zero, modulation, riverLimit);
            float endSlope = slope(atZero, atMax, zero, 1.0F);
            spline.addPoint(zero - 0.01F, atZero);
            spline.addPoint(zero, atZero, endSlope);
            spline.addPoint(1.0F, atMax, endSlope);
        } else {
            float straight = slope(atMin, atMax, -1.0F, 1.0F);
            if (saddle) {
                spline.addPoint(-1.0F, Math.max(0.2F, atMin));
                spline.addPoint(0.0F, Mth.lerp(0.5F, atMin, atMax), straight);
            } else {
                spline.addPoint(-1.0F, atMin, straight);
            }
            spline.addPoint(1.0F, atMax, straight);
        }
        return spline.build();
    }

    private static float mountainContinentalness(float ridge, float modulation, float riverLimit) {
        float slope = 1.0F - (1.0F - modulation) * 0.5F;
        float intersect = 0.5F * (1.0F - modulation);
        float value = (ridge + 1.17F) * 0.46082947F * slope - intersect;
        return ridge < riverLimit ? Math.max(value, -0.2222F) : Math.max(value, 0.0F);
    }

    private static float ridgeZeroPoint(float modulation) {
        float slope = 1.0F - (1.0F - modulation) * 0.5F;
        float intersect = 0.5F * (1.0F - modulation);
        return intersect / (0.46082947F * slope) - 1.17F;
    }

    public static <I extends BoundedFloatFunction<?>> CubicSpline<I> buildErosionOffsetSpline(I erosion, I ridges, float lowValley, float hill, float tallHill, float mountainFactor, float plain, float swamp, boolean includeExtremeHills, boolean saddle, Float2FloatFunction t) {
        CubicSpline<I> veryLow = mountainRidges(ridges, Mth.lerp(mountainFactor, 0.6F, 1.5F), saddle, t);
        CubicSpline<I> low = mountainRidges(ridges, Mth.lerp(mountainFactor, 0.6F, 1.0F), saddle, t);
        CubicSpline<I> mountains = mountainRidges(ridges, mountainFactor, saddle, t);
        CubicSpline<I> widePlateau = ridgeSpline(ridges, lowValley - 0.15F, 0.5F * mountainFactor, Mth.lerp(0.5F, 0.5F, 0.5F) * mountainFactor, 0.5F * mountainFactor, 0.6F * mountainFactor, 0.5F, t);
        CubicSpline<I> narrowPlateau = ridgeSpline(ridges, lowValley, plain * mountainFactor, hill * mountainFactor, 0.5F * mountainFactor, 0.6F * mountainFactor, 0.5F, t);
        CubicSpline<I> plains = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5F, t);
        CubicSpline<I> extremeHills = CubicSpline.<I>builder(ridges, t).addPoint(-1.0F, lowValley).addPoint(-0.4F, plains).addPoint(0.0F, tallHill + 0.07F).build();
        CubicSpline<I> swamps = ridgeSpline(ridges, -0.02F, swamp, swamp, hill, tallHill, 0.0F, t);
        CubicSpline.Builder<I> builder = CubicSpline.<I>builder(erosion, t)
            .addPoint(-0.85F, veryLow).addPoint(-0.7F, low).addPoint(-0.4F, mountains)
            .addPoint(-0.35F, widePlateau).addPoint(-0.1F, narrowPlateau).addPoint(0.2F, plains);
        if (includeExtremeHills) {
            builder.addPoint(0.4F, plains).addPoint(0.45F, extremeHills).addPoint(0.55F, extremeHills).addPoint(0.58F, plains);
        }
        builder.addPoint(0.7F, swamps);
        return builder.build();
    }

    private static <I extends BoundedFloatFunction<?>> CubicSpline<I> ridgeSpline(I ridges, float valley, float low, float mid, float high, float peaks, float minValleySteepness, Float2FloatFunction t) {
        float d1 = Math.max(0.5F * (low - valley), minValleySteepness);
        float d2 = 5.0F * (mid - low);
        return CubicSpline.<I>builder(ridges, t)
            .addPoint(-1.0F, valley, d1).addPoint(-0.4F, low, Math.min(d1, d2)).addPoint(0.0F, mid, d2)
            .addPoint(0.4F, high, 2.0F * (high - mid)).addPoint(1.0F, peaks, 0.7F * (peaks - high)).build();
    }
}
