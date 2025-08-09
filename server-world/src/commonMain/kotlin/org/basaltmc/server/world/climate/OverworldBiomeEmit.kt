package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder. The three shapes vanilla emits
 * a `(ParameterPoint, Biome)` pair in: surface (a `depth` pair spanning `[0, 1]`), underground
 * (a `depth` span of `[0.2, 0.9]`) and bottom (a single `depth` point at `1.1`).
 */
internal fun addSurfaceBiome(
    emit: (ParameterPoint, Biome) -> Unit,
    temperature: Parameter,
    humidity: Parameter,
    continentalness: Parameter,
    erosion: Parameter,
    weirdness: Parameter,
    offset: Float,
    biome: Biome,
) {
    emit(
        ParameterPoint(
            temperature,
            humidity,
            continentalness,
            erosion,
            Parameter.point(0.0f),
            weirdness,
            Parameter.quantizeCoord(offset),
        ),
        biome,
    )
    emit(
        ParameterPoint(
            temperature,
            humidity,
            continentalness,
            erosion,
            Parameter.point(1.0f),
            weirdness,
            Parameter.quantizeCoord(offset),
        ),
        biome,
    )
}

internal fun addUndergroundBiome(
    emit: (ParameterPoint, Biome) -> Unit,
    temperature: Parameter,
    humidity: Parameter,
    continentalness: Parameter,
    erosion: Parameter,
    weirdness: Parameter,
    offset: Float,
    biome: Biome,
) {
    emit(
        ParameterPoint(
            temperature,
            humidity,
            continentalness,
            erosion,
            Parameter.span(0.2f, 0.9f),
            weirdness,
            Parameter.quantizeCoord(offset),
        ),
        biome,
    )
}

internal fun addBottomBiome(
    emit: (ParameterPoint, Biome) -> Unit,
    temperature: Parameter,
    humidity: Parameter,
    continentalness: Parameter,
    erosion: Parameter,
    weirdness: Parameter,
    offset: Float,
    biome: Biome,
) {
    emit(
        ParameterPoint(
            temperature,
            humidity,
            continentalness,
            erosion,
            Parameter.point(1.1f),
            weirdness,
            Parameter.quantizeCoord(offset),
        ),
        biome,
    )
}
