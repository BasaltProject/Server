package org.basaltmc.world.densityfunction

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.TwoArgumentSimpleFunction.Type. */
public enum class Ap2Type {
    ADD,
    MUL,
    MIN,
    MAX,
}

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Ap2 (plus the bounds-computation
 * half of `TwoArgumentSimpleFunction.create`). A binary arithmetic combinator over two child
 * functions. Vanilla's `create` also special-cases a constant argument into a separate `MulOrAdd`
 * node; that is a representation/performance optimization only (mathematically identical result),
 * so it is not ported, matching this project's own precedent for `Climate`'s R-tree.
 */
public class Ap2 private constructor(
    public val type: Ap2Type,
    public val argument1: DensityFunction,
    public val argument2: DensityFunction,
    private val cachedMinValue: Double,
    private val cachedMaxValue: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double {
        val v1 = argument1.compute(context)
        return when (type) {
            Ap2Type.ADD -> v1 + argument2.compute(context)
            Ap2Type.MUL -> if (v1 == 0.0) 0.0 else v1 * argument2.compute(context)
            Ap2Type.MIN -> if (v1 < argument2.minValue()) v1 else minOf(v1, argument2.compute(context))
            Ap2Type.MAX -> if (v1 > argument2.maxValue()) v1 else maxOf(v1, argument2.compute(context))
        }
    }

    override fun minValue(): Double = cachedMinValue

    override fun maxValue(): Double = cachedMaxValue

    public companion object {
        public fun create(
            type: Ap2Type,
            argument1: DensityFunction,
            argument2: DensityFunction,
        ): Ap2 {
            val min1 = argument1.minValue()
            val min2 = argument2.minValue()
            val max1 = argument1.maxValue()
            val max2 = argument2.maxValue()
            val minValue =
                when (type) {
                    Ap2Type.ADD -> min1 + min2
                    Ap2Type.MUL ->
                        when {
                            min1 > 0.0 && min2 > 0.0 -> min1 * min2
                            max1 < 0.0 && max2 < 0.0 -> max1 * max2
                            else -> minOf(min1 * max2, max1 * min2)
                        }
                    Ap2Type.MIN -> minOf(min1, min2)
                    Ap2Type.MAX -> maxOf(min1, min2)
                }
            val maxValue =
                when (type) {
                    Ap2Type.ADD -> max1 + max2
                    Ap2Type.MUL ->
                        when {
                            min1 > 0.0 && min2 > 0.0 -> max1 * max2
                            max1 < 0.0 && max2 < 0.0 -> min1 * min2
                            else -> maxOf(min1 * min2, max1 * max2)
                        }
                    Ap2Type.MIN -> minOf(max1, max2)
                    Ap2Type.MAX -> maxOf(max1, max2)
                }
            return Ap2(type, argument1, argument2, minValue, maxValue)
        }
    }
}
