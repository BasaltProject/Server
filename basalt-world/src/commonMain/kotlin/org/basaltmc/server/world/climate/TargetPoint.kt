package org.basaltmc.server.world.climate

/** // parity: net.minecraft.world.level.biome.Climate.TargetPoint. A single sampled climate point, quantized. */
public data class TargetPoint(
    val temperature: Long,
    val humidity: Long,
    val continentalness: Long,
    val erosion: Long,
    val depth: Long,
    val weirdness: Long,
) {
    /** The offset axis has no target value: it is always compared against 0, matching vanilla's own 7th entry. */
    public fun toParameterArray(): LongArray =
        longArrayOf(temperature, humidity, continentalness, erosion, depth, weirdness, 0L)
}

public fun target(
    temperature: Float,
    humidity: Float,
    continentalness: Float,
    erosion: Float,
    depth: Float,
    weirdness: Float,
): TargetPoint =
    TargetPoint(
        Parameter.quantizeCoord(temperature),
        Parameter.quantizeCoord(humidity),
        Parameter.quantizeCoord(continentalness),
        Parameter.quantizeCoord(erosion),
        Parameter.quantizeCoord(depth),
        Parameter.quantizeCoord(weirdness),
    )
