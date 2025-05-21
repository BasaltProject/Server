package org.basaltmc.world.climate

/**
 * // parity: net.minecraft.world.level.biome.Climate.Parameter. A quantized `[min, max]` span
 * along one of the seven climate axes (temperature, humidity, continentalness, erosion, depth,
 * weirdness, offset). Values are stored pre-multiplied by [QUANTIZATION_FACTOR] and truncated to
 * `Long`, matching vanilla exactly (including its rounding behavior: truncation toward zero, not
 * [kotlin.math.round]).
 */
public data class Parameter(
    val min: Long,
    val max: Long,
) {
    public fun distance(target: Long): Long {
        val above = target - max
        val below = min - target
        return if (above > 0L) above else maxOf(below, 0L)
    }

    public fun distance(target: Parameter): Long {
        val above = target.min - max
        val below = min - target.max
        return if (above > 0L) above else maxOf(below, 0L)
    }

    public fun span(other: Parameter?): Parameter =
        if (other == null) this else Parameter(minOf(min, other.min), maxOf(max, other.max))

    public companion object {
        private const val QUANTIZATION_FACTOR = 10000.0f

        public fun quantizeCoord(coord: Float): Long = (coord * QUANTIZATION_FACTOR).toLong()

        public fun unquantizeCoord(coord: Long): Float = coord / QUANTIZATION_FACTOR

        public fun point(value: Float): Parameter = span(value, value)

        public fun span(
            min: Float,
            max: Float,
        ): Parameter {
            require(min <= max) { "min > max: $min $max" }
            return Parameter(quantizeCoord(min), quantizeCoord(max))
        }

        public fun span(
            min: Parameter,
            max: Parameter,
        ): Parameter {
            require(min.min <= max.max) { "min > max: $min $max" }
            return Parameter(min.min, max.max)
        }
    }
}
