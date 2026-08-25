package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder. The off-coast oceans, the
 * valley (river) slice and the cave/deep-dark underground biomes: the parts of `addBiomes` that
 * do not fit the peaks/high/mid/low slice shape.
 */
internal fun OverworldClimateTables.addOffCoastBiomes(emit: (ParameterPoint, Biome) -> Unit) {
    addSurfaceBiome(
        emit,
        fullRange,
        fullRange,
        mushroomFieldsContinentalness,
        fullRange,
        fullRange,
        0.0f,
        Biomes.MUSHROOM_FIELDS,
    )
    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        addSurfaceBiome(
            emit,
            temperature,
            fullRange,
            deepOceanContinentalness,
            fullRange,
            fullRange,
            0.0f,
            oceans[0][t],
        )
        addSurfaceBiome(
            emit,
            temperature,
            fullRange,
            oceanContinentalness,
            fullRange,
            fullRange,
            0.0f,
            oceans[1][t],
        )
    }
}

internal fun OverworldClimateTables.addValleys(
    emit: (ParameterPoint, Biome) -> Unit,
    weirdness: Parameter,
) {
    addSurfaceBiome(
        emit,
        frozenRange,
        fullRange,
        coastContinentalness,
        Parameter.span(erosions[0], erosions[1]),
        weirdness,
        0.0f,
        if (weirdness.max < 0L) Biomes.STONY_SHORE else Biomes.FROZEN_RIVER,
    )
    addSurfaceBiome(
        emit,
        unfrozenRange,
        fullRange,
        coastContinentalness,
        Parameter.span(erosions[0], erosions[1]),
        weirdness,
        0.0f,
        if (weirdness.max < 0L) Biomes.STONY_SHORE else Biomes.RIVER,
    )
    addSurfaceBiome(
        emit,
        frozenRange,
        fullRange,
        nearInlandContinentalness,
        Parameter.span(erosions[0], erosions[1]),
        weirdness,
        0.0f,
        Biomes.FROZEN_RIVER,
    )
    addSurfaceBiome(
        emit,
        unfrozenRange,
        fullRange,
        nearInlandContinentalness,
        Parameter.span(erosions[0], erosions[1]),
        weirdness,
        0.0f,
        Biomes.RIVER,
    )
    addSurfaceBiome(
        emit,
        frozenRange,
        fullRange,
        Parameter.span(coastContinentalness, farInlandContinentalness),
        Parameter.span(erosions[2], erosions[5]),
        weirdness,
        0.0f,
        Biomes.FROZEN_RIVER,
    )
    addSurfaceBiome(
        emit,
        unfrozenRange,
        fullRange,
        Parameter.span(coastContinentalness, farInlandContinentalness),
        Parameter.span(erosions[2], erosions[5]),
        weirdness,
        0.0f,
        Biomes.RIVER,
    )
    addSurfaceBiome(
        emit,
        frozenRange,
        fullRange,
        coastContinentalness,
        erosions[6],
        weirdness,
        0.0f,
        Biomes.FROZEN_RIVER,
    )
    addSurfaceBiome(
        emit,
        unfrozenRange,
        fullRange,
        coastContinentalness,
        erosions[6],
        weirdness,
        0.0f,
        Biomes.RIVER,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[1], temperatures[2]),
        fullRange,
        Parameter.span(inlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.SWAMP,
    )
    addSurfaceBiome(
        emit,
        Parameter.span(temperatures[3], temperatures[4]),
        fullRange,
        Parameter.span(inlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.MANGROVE_SWAMP,
    )
    addSurfaceBiome(
        emit,
        frozenRange,
        fullRange,
        Parameter.span(inlandContinentalness, farInlandContinentalness),
        erosions[6],
        weirdness,
        0.0f,
        Biomes.FROZEN_RIVER,
    )

    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        for (h in humidities.indices) {
            val humidity = humidities[h]
            val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                Parameter.span(erosions[0], erosions[1]),
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
        }
    }
}

internal fun OverworldClimateTables.addUndergroundBiomes(emit: (ParameterPoint, Biome) -> Unit) {
    addUndergroundBiome(
        emit,
        fullRange,
        fullRange,
        Parameter.span(0.8f, 1.0f),
        fullRange,
        fullRange,
        0.0f,
        Biomes.DRIPSTONE_CAVES,
    )
    addUndergroundBiome(
        emit,
        fullRange,
        Parameter.span(0.7f, 1.0f),
        fullRange,
        fullRange,
        fullRange,
        0.0f,
        Biomes.LUSH_CAVES,
    )
    addUndergroundBiome(
        emit,
        fullRange,
        fullRange,
        Parameter.span(coastContinentalness, inlandContinentalness),
        Parameter.span(erosions[5], erosions[6]),
        Parameter.span(-1.1f, -0.85f),
        0.0f,
        Biomes.SULFUR_CAVES,
    )
    addBottomBiome(
        emit,
        fullRange,
        fullRange,
        fullRange,
        Parameter.span(erosions[0], erosions[1]),
        fullRange,
        0.0f,
        Biomes.DEEP_DARK,
    )
}
