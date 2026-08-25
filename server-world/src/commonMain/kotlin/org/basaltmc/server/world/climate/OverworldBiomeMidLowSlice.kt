package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder. The mid- and low-erosion
 * inland weirdness slices of `addInlandBiomes`.
 */
internal fun OverworldClimateTables.addMidSlice(
    emit: (ParameterPoint, Biome) -> Unit,
    weirdness: Parameter,
) {
    addSurfaceBiome(
        emit,
        fullRange,
        fullRange,
        coastContinentalness,
        Parameter.span(erosions[0], erosions[2]),
        weirdness,
        0.0f,
        Biomes.STONY_SHORE,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[1], temperatures[2]),
        fullRange,
        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.SWAMP,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[3], temperatures[4]),
        fullRange,
        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.MANGROVE_SWAMP,
    )

    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        for (h in humidities.indices) {
            val humidity = humidities[h]
            val middleBiome = pickMiddleBiome(t, h, weirdness)
            val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
            val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
            val shatteredBiome = pickShatteredBiome(t, h, weirdness)
            val plateauBiome = pickPlateauBiome(t, h, weirdness)
            val beachBiome = pickBeachBiome(t, h)
            val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
            val shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness)
            val slopeBiome = pickSlopeBiome(t, h, weirdness)
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                erosions[0],
                weirdness,
                0.0f,
                slopeBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(nearInlandContinentalness, midInlandContinentalness),
                erosions[1],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHotOrSlopeIfCold,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                farInlandContinentalness,
                erosions[1],
                weirdness,
                0.0f,
                if (t == 0) slopeBiome else plateauBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                erosions[2],
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                midInlandContinentalness,
                erosions[2],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                farInlandContinentalness,
                erosions[2],
                weirdness,
                0.0f,
                plateauBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
                erosions[3],
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[3],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            if (weirdness.max < 0L) {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[4],
                    weirdness,
                    0.0f,
                    beachBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                    erosions[4],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            } else {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[4],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                coastContinentalness,
                erosions[5],
                weirdness,
                0.0f,
                shatteredCoastBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                erosions[5],
                weirdness,
                0.0f,
                middleBiomeOrWindsweptSavanna,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[5],
                weirdness,
                0.0f,
                shatteredBiome,
            )
            if (weirdness.max < 0L) {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[6],
                    weirdness,
                    0.0f,
                    beachBiome,
                )
            } else {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[6],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
            if (t == 0) {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                    erosions[6],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
        }
    }
}

internal fun OverworldClimateTables.addLowSlice(
    emit: (ParameterPoint, Biome) -> Unit,
    weirdness: Parameter,
) {
    addSurfaceBiome(
        emit,
        fullRange,
        fullRange,
        coastContinentalness,
        Parameter.span(erosions[0], erosions[2]),
        weirdness,
        0.0f,
        Biomes.STONY_SHORE,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[1], temperatures[2]),
        fullRange,
        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.SWAMP,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[3], temperatures[4]),
        fullRange,
        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.MANGROVE_SWAMP,
    )

    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        for (h in humidities.indices) {
            val humidity = humidities[h]
            val middleBiome = pickMiddleBiome(t, h, weirdness)
            val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
            val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
            val beachBiome = pickBeachBiome(t, h)
            val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
            val shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness)
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                Parameter.span(erosions[0], erosions[1]),
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                Parameter.span(erosions[0], erosions[1]),
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHotOrSlopeIfCold,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                Parameter.span(erosions[2], erosions[3]),
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                Parameter.span(erosions[2], erosions[3]),
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                coastContinentalness,
                Parameter.span(erosions[3], erosions[4]),
                weirdness,
                0.0f,
                beachBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                erosions[4],
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                coastContinentalness,
                erosions[5],
                weirdness,
                0.0f,
                shatteredCoastBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                erosions[5],
                weirdness,
                0.0f,
                middleBiomeOrWindsweptSavanna,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[5],
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                coastContinentalness,
                erosions[6],
                weirdness,
                0.0f,
                beachBiome,
            )
            if (t == 0) {
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                    erosions[6],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
        }
    }
}
