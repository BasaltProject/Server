package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/** One biome's real `vegetal_decoration` (and friends) generation step. // parity: the `Decoration.VEGETAL_DECORATION`-and-later portion of a biome's own `BiomeGenerationSettings`. */
public fun interface BiomeVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    )
}

/**
 * Dispatches a biome identifier to its real decoration, matching real vanilla's own
 * `ChunkGenerator.applyBiomeDecoration` (which looks up the actual biome at each decorated
 * position, not a single fixed one). [org.basaltmc.world.terrain.RealTerrainGenerator] resolves
 * only the chunk's own center column and decorates the whole chunk with that one result: real
 * vanilla's decoration is chunk-scoped in practice too (everything placed by one chunk's
 * `applyBiomeDecoration` call shares that chunk's carved region), and this project's own decoration
 * pipeline is already chunk-scoped ([PlacementContext][org.basaltmc.world.terrain.decoration.PlacementContext]
 * has no cross-chunk state), so per-column dispatch is not meaningfully different except at biome
 * borders, an accepted, documented approximation.
 *
 * Only biomes with real, jar-verified decoration ported so far are listed: every other biome
 * resolves to `null` (no vegetal decoration at all), strictly more correct than running a
 * different biome's own feature list over it.
 */
public object BiomeDecoration {
    private val byBiome: Map<Identifier, BiomeVegetalDecoration> =
        mapOf(
            Identifier.vanilla("plains") to PlainsDecoration,
        )

    public fun forBiome(id: Identifier): BiomeVegetalDecoration? = byBiome[id]
}
