package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.world.noise.PerlinSimplexNoise
import org.basaltmc.server.world.noise.SimplexNoise

/**
 * // parity: net.minecraft.world.level.biome.Biome.coldEnoughToSnow/getHeightAdjustedTemperature and
 * Biome.TemperatureModifier.FROZEN.modifyTemperature. [TEMPERATURE_NOISE]/[FROZEN_TEMPERATURE_NOISE]/
 * [BIOME_INFO_NOISE] are real vanilla's own fixed-seed noises (`Biome`'s own static fields):
 * constructed straight from [LegacyRandom], not `WorldgenRandom`-wrapped, since `WorldgenRandom`
 * degenerates to a pure passthrough over a bit-based inner source anyway (already the established
 * simplification this project uses for the same real seeds, see `PlainsDecoration`'s own
 * `FLOWER_NOISE`/`BIOME_INFO_NOISE` note).
 */
public object BiomeTemperature {
    // Vanilla's own Overworld noise settings constant (`sea_level` in `overworld.json`), also
    // already hardcoded elsewhere in this project's own SurfaceRuleData (`aboveOverworldSeaLevel`).
    private const val SEA_LEVEL = 63

    private val TEMPERATURE_NOISE = PerlinSimplexNoise(LegacyRandom(1234L), listOf(0))
    private val FROZEN_TEMPERATURE_NOISE = PerlinSimplexNoise(LegacyRandom(3456L), listOf(-2, -1, 0))
    private val BIOME_INFO_NOISE = SimplexNoise(LegacyRandom(2345L))

    /** // parity: Biome.coldEnoughToSnow(BlockPos, int): `getTemperature(pos, seaLevel) < 0.15F`. */
    public fun coldEnoughToSnow(
        biomeId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = temperatureAt(biomeId, x, y, z) < 0.15f

    /** // parity: `Biome.shouldMeltFrozenOceanIcebergSlightly`: temperature greater than `0.1`. */
    public fun shouldMeltFrozenOceanIcebergSlightly(
        biomeId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = temperatureAt(biomeId, x, y, z) > 0.1f

    private fun temperatureAt(
        biomeId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Float {
        val climate = BiomeClimateData.forBiomeId(biomeId)
        val adjusted = if (climate.frozen) frozenModifiedTemperature(x, z, climate.temperature) else climate.temperature
        val snowLevel = SEA_LEVEL + 17
        if (y <= snowLevel) return adjusted
        val v = TEMPERATURE_NOISE.getValue(x / 8.0, z / 8.0) * 8.0
        return adjusted - ((v + y - snowLevel) * 0.05 / 40.0).toFloat()
    }

    // parity: Biome.TemperatureModifier.FROZEN.modifyTemperature.
    private fun frozenModifiedTemperature(
        x: Int,
        z: Int,
        baseTemperature: Float,
    ): Float {
        val groundValueLargeVariation = FROZEN_TEMPERATURE_NOISE.getValue(x * 0.05, z * 0.05) * 7.0
        val groundValueEdgeVariation = BIOME_INFO_NOISE.getValue(x * 0.2, z * 0.2)
        val icePatches = groundValueLargeVariation + groundValueEdgeVariation
        if (icePatches < 0.3) {
            val groundValueSmallVariation = BIOME_INFO_NOISE.getValue(x * 0.09, z * 0.09)
            if (groundValueSmallVariation < 0.8) return 0.2f
        }
        return baseTemperature
    }
}
