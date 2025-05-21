package org.basaltmc.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.IntervalSelect. Picks one of
 * [functions] by comparing [input]'s value against ascending [thresholds]: the first threshold
 * [input] is strictly less than selects that bucket, falling through to the last function if
 * [input] is at or above every threshold. `functions.size` must be `thresholds.size + 1`.
 */
public class IntervalSelect(
    public val input: DensityFunction,
    public val thresholds: DoubleArray,
    public val functions: List<DensityFunction>,
) : DensityFunction {
    init {
        require(thresholds.size == functions.size - 1) {
            "Expected ${functions.size - 1} thresholds for ${functions.size} functions, but got ${thresholds.size}"
        }
        for (i in 1 until thresholds.size) {
            require(thresholds[i - 1] <= thresholds[i]) { "Threshold values must be ordered from smallest to largest" }
        }
    }

    override fun compute(context: FunctionContext): Double = select(context, input.compute(context))

    private fun select(
        context: FunctionContext,
        inputValue: Double,
    ): Double {
        for (i in thresholds.indices) {
            if (inputValue < thresholds[i]) return functions[i].compute(context)
        }
        return functions.last().compute(context)
    }

    override fun minValue(): Double = functions.minOf { it.minValue() }

    override fun maxValue(): Double = functions.maxOf { it.maxValue() }
}
