package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.world.climate.OverworldBiomeBuilder
import org.basaltmc.server.world.climate.ParameterList
import org.basaltmc.server.world.climate.Sampler
import org.basaltmc.server.world.densityfunction.DensityCache

/**
 * // parity: net.minecraft.world.level.biome.MultiNoiseBiomeSource, specialized to the Overworld
 * preset. Samples [router]'s six climate functions at quart resolution and takes the closest
 * registered climate region.
 *
 * Holds no cache on purpose: one instance is shared by every chunk worker, and a plain map
 * corrupted itself under concurrent writes here. Copy-on-write is quadratic given how many quart
 * positions a chunk touches, so memoization belongs one level up, in a per-chunk single-threaded
 * cache like `RealTerrainGenerator`'s per-chunk biome environment.
 */
public class OverworldBiomeSource(
    router: OverworldNoiseRouter,
    biomeZoomSeed: Long? = null,
) {
    private val zoomer = biomeZoomSeed?.let(::BiomeZoomer)
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

    /** Direct noise-biome lookup from block coordinates, without BiomeManager zoom. */
    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache? = null,
    ): Biome = biomeAtQuart(x shr 2, y shr 2, z shr 2, cache)

    public fun biomeAtQuart(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        cache: DensityCache? = null,
    ): Biome {
        val target = sampler.sample(quartX, quartY, quartZ, cache)
        return parameters.findValue(target)
    }

    /** // parity: BiomeManager.getBiome, used by block-position gameplay and worldgen filters. */
    public fun blockBiomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache? = null,
    ): Biome =
        zoomer?.biomeAt(x, y, z) { quartX, quartY, quartZ -> biomeAtQuart(quartX, quartY, quartZ, cache) }
            ?: biomeAt(x, y, z, cache)
}
