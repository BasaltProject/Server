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
 * cache like `RealTerrainGenerator`'s `ChunkScopedSurfaceEnvironment`.
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
        cache: DensityCache? = null,
    ): Biome {
        val target = sampler.sample(x shr 2, y shr 2, z shr 2, cache)
        return parameters.findValue(target)
    }
}
