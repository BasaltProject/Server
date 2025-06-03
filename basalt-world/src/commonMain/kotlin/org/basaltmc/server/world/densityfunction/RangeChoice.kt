package org.basaltmc.server.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.RangeChoice. Picks [whenInRange]
 * or [whenOutOfRange] depending on whether [input] falls in `[minInclusive, maxExclusive)`.
 */
public class RangeChoice(
    public val input: DensityFunction,
    public val minInclusive: Double,
    public val maxExclusive: Double,
    public val whenInRange: DensityFunction,
    public val whenOutOfRange: DensityFunction,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double {
        val inputValue = input.compute(context)
        return if (inputValue >= minInclusive && inputValue < maxExclusive) {
            whenInRange.compute(context)
        } else {
            whenOutOfRange.compute(context)
        }
    }

    override fun minValue(): Double = minOf(whenInRange.minValue(), whenOutOfRange.minValue())

    override fun maxValue(): Double = maxOf(whenInRange.maxValue(), whenOutOfRange.maxValue())
}
