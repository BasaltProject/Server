package org.basaltmc.server.world.densityfunction

import org.basaltmc.wire.common.Mth

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Mapped.Type. */
public enum class MappedType {
    ABS,
    SQUARE,
    CUBE,
    HALF_NEGATIVE,
    QUARTER_NEGATIVE,
    INVERT,
    SQUEEZE,
}

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Mapped. A unary transform of
 * [input]. Bounds are derived once at construction (via [create]) by running [transform] on
 * [input]'s own declared bounds, same as vanilla; use [create], not the constructor directly, so
 * those bounds stay correct.
 */
public class Mapped private constructor(
    public val type: MappedType,
    public val input: DensityFunction,
    private val cachedMinValue: Double,
    private val cachedMaxValue: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double = transform(type, input.compute(context))

    override fun minValue(): Double = cachedMinValue

    override fun maxValue(): Double = cachedMaxValue

    public companion object {
        public fun create(
            type: MappedType,
            input: DensityFunction,
        ): Mapped {
            val minValue = input.minValue()
            val maxValue = input.maxValue()
            val minImage = transform(type, minValue)
            val maxImage = transform(type, maxValue)
            return when {
                type == MappedType.INVERT && minValue < 0.0 && maxValue > 0.0 ->
                    Mapped(type, input, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY)
                type == MappedType.INVERT -> Mapped(type, input, maxImage, minImage)
                type == MappedType.ABS || type == MappedType.SQUARE ->
                    Mapped(type, input, maxOf(0.0, minValue), maxOf(minImage, maxImage))
                else -> Mapped(type, input, minImage, maxImage)
            }
        }

        public fun transform(
            type: MappedType,
            input: Double,
        ): Double =
            when (type) {
                MappedType.ABS -> kotlin.math.abs(input)
                MappedType.SQUARE -> input * input
                MappedType.CUBE -> input * input * input
                MappedType.HALF_NEGATIVE -> if (input > 0.0) input else input * 0.5
                MappedType.QUARTER_NEGATIVE -> if (input > 0.0) input else input * 0.25
                MappedType.INVERT -> 1.0 / input
                MappedType.SQUEEZE -> {
                    val c = Mth.clamp(input, -1.0, 1.0)
                    c / 2.0 - c * c * c / 24.0
                }
            }
    }
}
