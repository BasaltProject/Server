package org.basaltmc.world.climate

import org.basaltmc.common.Mth

/**
 * // parity: net.minecraft.world.level.biome.Climate.ParameterPoint. One biome's climate niche:
 * an allowed span on each of the six sampled axes, plus a quantized `offset` (a 7th axis that
 * every [TargetPoint] implicitly targets at 0).
 */
public data class ParameterPoint(
    val temperature: Parameter,
    val humidity: Parameter,
    val continentalness: Parameter,
    val erosion: Parameter,
    val depth: Parameter,
    val weirdness: Parameter,
    val offset: Long,
) {
    /** Squared distance to [target]: lower is a better biome match. */
    public fun fitness(target: TargetPoint): Long =
        Mth.square(temperature.distance(target.temperature)) +
            Mth.square(humidity.distance(target.humidity)) +
            Mth.square(continentalness.distance(target.continentalness)) +
            Mth.square(erosion.distance(target.erosion)) +
            Mth.square(depth.distance(target.depth)) +
            Mth.square(weirdness.distance(target.weirdness)) +
            Mth.square(offset)

    internal fun parameterSpace(): List<Parameter> =
        listOf(temperature, humidity, continentalness, erosion, depth, weirdness, Parameter(offset, offset))
}

public fun parameters(
    temperature: Float,
    humidity: Float,
    continentalness: Float,
    erosion: Float,
    depth: Float,
    weirdness: Float,
    offset: Float,
): ParameterPoint =
    ParameterPoint(
        Parameter.point(temperature),
        Parameter.point(humidity),
        Parameter.point(continentalness),
        Parameter.point(erosion),
        Parameter.point(depth),
        Parameter.point(weirdness),
        Parameter.quantizeCoord(offset),
    )
