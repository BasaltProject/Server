package org.basaltmc.world.densityfunction

import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.spline.CubicSpline

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions (the static factory methods
 * only, not the node classes themselves: those already exist as their own files in this package).
 * Vanilla's `NoiseHolder` (a lazily-resolved registry reference) has no equivalent here: every
 * factory below takes an already-resolved [NormalNoise] directly, matching [Noise]'s own KDoc note.
 */
public object DensityFunctions {
    public fun zero(): DensityFunction = Constant(0.0)

    public fun constant(value: Double): DensityFunction = Constant(value)

    public fun add(
        a: DensityFunction,
        b: DensityFunction,
    ): DensityFunction = Ap2.create(Ap2Type.ADD, a, b)

    public fun mul(
        a: DensityFunction,
        b: DensityFunction,
    ): DensityFunction = Ap2.create(Ap2Type.MUL, a, b)

    public fun min(
        a: DensityFunction,
        b: DensityFunction,
    ): DensityFunction = Ap2.create(Ap2Type.MIN, a, b)

    public fun max(
        a: DensityFunction,
        b: DensityFunction,
    ): DensityFunction = Ap2.create(Ap2Type.MAX, a, b)

    public fun clamp(
        input: DensityFunction,
        minValue: Double,
        maxValue: Double,
    ): DensityFunction = Clamp(input, minValue, maxValue)

    public fun abs(input: DensityFunction): DensityFunction = Mapped.create(MappedType.ABS, input)

    public fun square(input: DensityFunction): DensityFunction = Mapped.create(MappedType.SQUARE, input)

    public fun cube(input: DensityFunction): DensityFunction = Mapped.create(MappedType.CUBE, input)

    public fun halfNegative(input: DensityFunction): DensityFunction = Mapped.create(MappedType.HALF_NEGATIVE, input)

    public fun quarterNegative(input: DensityFunction): DensityFunction =
        Mapped.create(MappedType.QUARTER_NEGATIVE, input)

    public fun invert(input: DensityFunction): DensityFunction = Mapped.create(MappedType.INVERT, input)

    public fun squeeze(input: DensityFunction): DensityFunction = Mapped.create(MappedType.SQUEEZE, input)

    public fun interpolated(input: DensityFunction): DensityFunction = Marker(MarkerType.INTERPOLATED, input)

    public fun flatCache(input: DensityFunction): DensityFunction = Marker(MarkerType.FLAT_CACHE, input)

    public fun cache2d(input: DensityFunction): DensityFunction = Marker(MarkerType.CACHE_2D, input)

    public fun cacheOnce(input: DensityFunction): DensityFunction = Marker(MarkerType.CACHE_ONCE, input)

    public fun cacheAllInCell(input: DensityFunction): DensityFunction = Marker(MarkerType.CACHE_ALL_IN_CELL, input)

    public fun blendDensity(input: DensityFunction): DensityFunction = Marker(MarkerType.BLEND_DENSITY, input)

    public fun blendAlpha(): DensityFunction = BlendAlpha

    public fun blendOffset(): DensityFunction = BlendOffset

    public fun yClampedGradient(
        fromY: Int,
        toY: Int,
        fromValue: Double,
        toValue: Double,
    ): DensityFunction = YClampedGradient(fromY, toY, fromValue, toValue)

    public fun noise(
        noise: NormalNoise,
        xzScale: Double = 1.0,
        yScale: Double = 1.0,
    ): DensityFunction = Noise(noise, xzScale, yScale)

    public fun mappedNoise(
        noise: NormalNoise,
        xzScale: Double = 1.0,
        yScale: Double = 1.0,
        minTarget: Double,
        maxTarget: Double,
    ): DensityFunction = mapFromUnitTo(Noise(noise, xzScale, yScale), minTarget, maxTarget)

    private fun mapFromUnitTo(
        function: DensityFunction,
        min: Double,
        max: Double,
    ): DensityFunction {
        val middle = (min + max) * 0.5
        val factor = (max - min) * 0.5
        return add(constant(middle), mul(constant(factor), function))
    }

    public fun shiftedNoise2d(
        shiftX: DensityFunction,
        shiftZ: DensityFunction,
        xzScale: Double,
        noise: NormalNoise,
    ): DensityFunction = ShiftedNoise(shiftX, Constant(0.0), shiftZ, xzScale, 0.0, noise)

    public fun shiftA(noise: NormalNoise): DensityFunction = ShiftA(noise)

    public fun shiftB(noise: NormalNoise): DensityFunction = ShiftB(noise)

    public fun shift(noise: NormalNoise): DensityFunction = Shift(noise)

    public fun rangeChoice(
        input: DensityFunction,
        minInclusive: Double,
        maxExclusive: Double,
        whenInRange: DensityFunction,
        whenOutOfRange: DensityFunction,
    ): DensityFunction = RangeChoice(input, minInclusive, maxExclusive, whenInRange, whenOutOfRange)

    public fun intervalSelect(
        input: DensityFunction,
        thresholds: DoubleArray,
        functions: List<DensityFunction>,
    ): DensityFunction = IntervalSelect(input, thresholds, functions)

    public fun spline(spline: CubicSpline<FunctionContext, SplineCoordinate>): DensityFunction = Spline(spline)

    public fun endIslands(seed: Long): DensityFunction = EndIslandDensityFunction(seed)

    public fun findTopSurface(
        density: DensityFunction,
        upperBound: DensityFunction,
        lowerBound: Int,
        cellHeight: Int,
    ): DensityFunction = FindTopSurface(density, upperBound, lowerBound, cellHeight)

    /** The `(DensityFunction, DensityFunction, DensityFunction)` overload: special-cases a constant [first]. */
    public fun lerp(
        alpha: DensityFunction,
        first: DensityFunction,
        second: DensityFunction,
    ): DensityFunction {
        if (first is Constant) return lerp(alpha, first.value, second)
        val alphaCached = cacheOnce(alpha)
        val oneMinusAlpha = add(mul(alphaCached, constant(-1.0)), constant(1.0))
        return add(mul(first, oneMinusAlpha), mul(second, alphaCached))
    }

    /** The `(DensityFunction, double, DensityFunction)` overload [slide] and friends actually call. */
    public fun lerp(
        factor: DensityFunction,
        first: Double,
        second: DensityFunction,
    ): DensityFunction = add(mul(factor, add(second, constant(-first))), constant(first))
}
