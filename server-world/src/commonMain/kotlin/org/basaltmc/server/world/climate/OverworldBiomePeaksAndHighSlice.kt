package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder. The peaks and high-erosion
 * inland weirdness slices of `addInlandBiomes`.
 */
internal fun OverworldClimateTables.addPeaks(
    emit: (ParameterPoint, Biome) -> Unit,
    weirdness: Parameter,
) {
    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        for (h in humidities.indices) {
            val humidity = humidities[h]
            val middleBiome = pickMiddleBiome(t, h, weirdness)
            val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
            val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
            val plateauBiome = pickPlateauBiome(t, h, weirdness)
            val shatteredBiome = pickShatteredBiome(t, h, weirdness)
            val shatteredBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, shatteredBiome)
            val peakBiome = pickPeakBiome(t, h, weirdness)
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, farInlandContinentalness),
                erosions[0],
                weirdness,
                0.0f,
                peakBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
                erosions[1],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHotOrSlopeIfCold,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[1],
                weirdness,
                0.0f,
                peakBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
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
                erosions[2],
                weirdness,
                0.0f,
                plateauBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                midInlandContinentalness,
                erosions[3],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                farInlandContinentalness,
                erosions[3],
                weirdness,
                0.0f,
                plateauBiome,
            )
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
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
                erosions[5],
                weirdness,
                0.0f,
                shatteredBiomeOrWindsweptSavanna,
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
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, farInlandContinentalness),
                erosions[6],
                weirdness,
                0.0f,
                middleBiome,
            )
        }
    }
}

internal fun OverworldClimateTables.addHighSlice(
    emit: (ParameterPoint, Biome) -> Unit,
    weirdness: Parameter,
) {
    for (t in temperatures.indices) {
        val temperature = temperatures[t]
        for (h in humidities.indices) {
            val humidity = humidities[h]
            val middleBiome = pickMiddleBiome(t, h, weirdness)
            val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
            val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
            val plateauBiome = pickPlateauBiome(t, h, weirdness)
            val shatteredBiome = pickShatteredBiome(t, h, weirdness)
            val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
            val slopeBiome = pickSlopeBiome(t, h, weirdness)
            val peakBiome = pickPeakBiome(t, h, weirdness)
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                coastContinentalness,
                Parameter.span(erosions[0], erosions[1]),
                weirdness,
                0.0f,
                middleBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                erosions[0],
                weirdness,
                0.0f,
                slopeBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[0],
                weirdness,
                0.0f,
                peakBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                nearInlandContinentalness,
                erosions[1],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHotOrSlopeIfCold,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(midInlandContinentalness, farInlandContinentalness),
                erosions[1],
                weirdness,
                0.0f,
                slopeBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
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
                erosions[2],
                weirdness,
                0.0f,
                plateauBiome,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                midInlandContinentalness,
                erosions[3],
                weirdness,
                0.0f,
                middleBiomeOrBadlandsIfHot,
            )
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                farInlandContinentalness,
                erosions[3],
                weirdness,
                0.0f,
                plateauBiome,
            )
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
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, nearInlandContinentalness),
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
            addSurfaceBiome(
                emit,
                temperature,
                humidity,
                Parameter.span(coastContinentalness, farInlandContinentalness),
                erosions[6],
                weirdness,
                0.0f,
                middleBiome,
            )
        }
    }
}
