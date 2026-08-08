package org.basaltmc.world.terrain

import org.basaltmc.registry.Biome
import org.basaltmc.world.climate.OverworldBiomeBuilder
import org.basaltmc.world.climate.ParameterList
import org.basaltmc.world.climate.Sampler

/**
 * // parity: net.minecraft.world.level.biome.MultiNoiseBiomeSource, specialized to the real
 * Overworld preset (`MultiNoiseBiomeSourceParameterLists.OVERWORLD`, itself just
 * `new OverworldBiomeBuilder().addBiomes(...)`, see [OverworldBiomeBuilder]'s own KDoc). Resolves a
 * block position to its real vanilla biome by sampling [router]'s six climate density functions at
 * that position's quart (4-block) resolution and finding the closest registered climate region.
 *
 * Deliberately holds no cache of its own: a single [OverworldBiomeSource] is shared across every
 * worker thread generating chunks in parallel
 * ([org.basaltmc.server.BasaltServer.generateChunksInParallel]), and this method is called many
 * times per column (real vanilla's own [org.basaltmc.world.terrain.surface.SurfaceRules.isBiome]
 * re-samples the biome independently for every check in a sequence, not once per position). A
 * plain shared `MutableMap` corrupted its own backing array under concurrent writes the first time
 * this ran live, and the safe copy-on-write alternative
 * ([VillageStructureGenerator.samplerCache]'s own established pattern) copies its entire backing
 * map on every insert, which is fine for that cache's rare inserts but quadratic here given how
 * many distinct quart positions one chunk alone touches. The right place to memoize calls to this
 * method is therefore one level up, in a short-lived, single-thread, per-chunk cache (see
 * [org.basaltmc.world.terrain.RealTerrainGenerator]'s own `ChunkScopedSurfaceEnvironment`), not
 * inside this long-lived, cross-thread-shared object.
 */
public class OverworldBiomeSource(
    router: OverworldNoiseRouter,
) {
    private val parameters: ParameterList<Biome> = OverworldBiomeBuilder().build()
    private val sampler =
        Sampler(
            temperature = router.temperature,
            humidity = router.vegetation,
            continentalness = router.continents,
            erosion = router.erosion,
            depth = router.depth,
            weirdness = router.ridges,
        )

    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): Biome {
        val target = sampler.sample(x shr 2, y shr 2, z shr 2)
        return parameters.findValue(target)
    }
}
