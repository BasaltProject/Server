package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.BiomeZoomer

/** Caches raw quart biomes while exposing BiomeManager's zoomed block-position lookup. */
internal class ChunkBiomeEnvironment(
    private val delegate: SurfaceRuleEnvironment,
    private val densityCache: DensityCache,
) : SurfaceRuleEnvironment by delegate {
    private val zoomer = delegate.biomeZoomSeed?.let(::BiomeZoomer)
    private val noiseBiomes = HashMap<Long, String>()

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): String =
        zoomer?.biomeAt(x, y, z) { quartX, quartY, quartZ ->
            noiseBiomeAt(quartX, quartY, quartZ, densityCache)
        } ?: delegate.biomeAt(x, y, z, densityCache)

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache?,
    ): String = biomeAt(x, y, z)

    override fun noiseBiomeAt(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        cache: DensityCache?,
    ): String {
        val key =
            (quartX.toLong() and 0x1FFFFFL) or
                ((quartY.toLong() and 0xFFFFFL) shl 21) or
                ((quartZ.toLong() and 0x1FFFFFL) shl 41)
        noiseBiomes[key]?.let { return it }
        val biome = delegate.noiseBiomeAt(quartX, quartY, quartZ, densityCache)
        noiseBiomes[key] = biome
        return biome
    }
}
