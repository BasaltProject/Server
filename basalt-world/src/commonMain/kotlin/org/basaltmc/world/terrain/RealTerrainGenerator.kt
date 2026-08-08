package org.basaltmc.world.terrain

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.Marker
import org.basaltmc.world.terrain.decoration.BiomeDecoration
import org.basaltmc.world.terrain.decoration.SurfaceFreezingDecoration
import org.basaltmc.world.terrain.surface.SurfaceBuilder
import org.basaltmc.world.terrain.surface.SurfaceRuleData
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment

/**
 * Fills a [Chunk]'s solid/air shape from a real [OverworldNoiseRouter]'s `finalDensity`, sampled
 * through [DensityChunkSampler] (real vanilla's own cell-interpolation scheme, not per-block
 * evaluation, which earlier this project's own `BlendedNoise` work found to be too slow to be
 * viable), then runs the real [SurfaceRuleData.overworld] rule tree over it via [SurfaceBuilder]
 * (grass/dirt/gravel/deepslate/bedrock, real per-biome rules, not a fixed-depth stand-in). One
 * [OverworldNoiseRouter] should be built once per world (it is expensive to construct: dozens of
 * nested [org.basaltmc.world.densityfunction.DensityFunction] nodes) and reused across every chunk.
 *
 * [environment] is wrapped with the chunk's own just-computed heightmap for
 * [SurfaceRuleEnvironment.estimatedSurfaceHeight]/[SurfaceRuleEnvironment.isSteep]: both only need
 * this one already-generated chunk's own shape (real vanilla's own equivalents are chunk-scoped
 * too, not global), and a cheap array lookup here instead of [environment] re-scanning the density
 * function from scratch matters a lot: measured at roughly 80x more density evaluations per chunk
 * than [DensityChunkSampler]'s own cell grid needs, an easy trap to fall back into after already
 * solving it once for the base terrain shape.
 *
 * Real vanilla places structures during their own generation step, before decoration features run
 * (so decoration naturally skips whatever blocks a structure already placed): when
 * [villageGenerator] is given, [VillageStructureGenerator.generateInto] runs right after surface
 * rules and before decoration, matching that order.
 *
 * After structures, [BiomeDecoration.forBiome] dispatches to whichever biome the chunk's own
 * center column resolves to (real vanilla decorates per chunk-section biome too, see
 * [BiomeDecoration]'s own KDoc for the simplification this project makes there), seeded with the
 * real `WorldgenRandom.setDecorationSeed([seed], chunkX * 16, chunkZ * 16)` formula, over the real
 * `WorldgenRandom`-wrapping-`XoroshiroRandomSource` combination `ChunkGenerator
 * .applyBiomeDecoration` itself uses (confirmed from its own decompiled body: the `seed` argument
 * there is the plain world seed, not a separately derived per-region seed).
 *
 * **Documented gaps, matching this project's own established scope for this pass:**
 * - No caves/local aquifers/ore veins: [OverworldNoiseRouter.finalDensity] itself already omits
 *   them (see its own KDoc). The global fluid picker's sea-level water is modeled.
 * - [BiomeDecoration] itself has its own documented per-biome gaps (see its own KDoc): only some
 *   biomes have real decoration ported so far, the rest fall back to no vegetal decoration at all
 *   (still strictly more correct than running another biome's decoration list on them).
 * - [villageGenerator] itself has its own documented gaps (see [VillageStructureGenerator]'s own
 *   KDoc): no terrain adaptation (buildings don't get their surrounding dirt "beard" carved to
 *   meet uneven ground), no block-state rotation on directional blocks, no entities.
 */
public object RealTerrainGenerator {
    private const val CELL_WIDTH = 4
    private const val CELL_HEIGHT = 8
    private const val SEA_LEVEL = 63

    public fun generate(
        router: OverworldNoiseRouter,
        environment: SurfaceRuleEnvironment,
        chunkX: Int,
        chunkZ: Int,
        seed: Long = 0L,
        villageGenerator: VillageStructureGenerator? = null,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        // One cache for the whole chunk, shared by terrain shape, surface rules and biome filling:
        // they all walk the same router subtrees, so a cache per phase would throw away most of it.
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                CELL_WIDTH,
                CELL_HEIGHT,
                cache,
            )
        GenerationMetrics.measure(GenerationMetrics.Phase.DENSITY_GRID) { sampler.prepare() }
        GenerationMetrics.measure(GenerationMetrics.Phase.TERRAIN_FILL) {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        if (sampler.sample(x, y, z) > 0.0) {
                            chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                        } else if (y < SEA_LEVEL) {
                            // parity: NoiseGeneratorSettings' global Overworld FluidPicker. Full
                            // aquifers are still absent, but outside caves their fallback is exactly
                            // water below sea level and air at/above it.
                            chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                        }
                    }
                }
            }
        }
        val heightmap = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
        val chunkEnvironment = ChunkScopedSurfaceEnvironment(environment, heightmap, cache)
        GenerationMetrics.measure(GenerationMetrics.Phase.SURFACE_RULES) {
            SurfaceBuilder.buildSurface(
                chunk,
                chunkEnvironment,
                SurfaceRuleData.overworld(),
            )
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.STRUCTURES) {
            villageGenerator?.generateInto(chunk, chunkX, chunkZ, seed)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.DECORATION) {
            val decorationRandom = WorldgenRandom(XoroshiroRandom(seed))
            decorationRandom.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
            val centerY = heightmap[8 + 8 * 16]
            val biomeId = Identifier.parse(chunkEnvironment.biomeAt(chunkX * 16 + 8, centerY, chunkZ * 16 + 8))
            BiomeDecoration.forBiome(biomeId)?.decorate(chunk, decorationRandom)
            SurfaceFreezingDecoration.decorate(chunk, chunkEnvironment)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.BIOME_FILL) { fillBiomes(chunk, chunkEnvironment) }
        if (GenerationMetrics.enabled) GenerationMetrics.countChunk()
        return chunk
    }

    /**
     * Fills every quart cell of the chunk with its real biome network ID, so the wire palette
     * ([org.basaltmc.world.ChunkDataCodec]) carries the actual biome layout the client needs for
     * grass/water/fog colors. Sampled once per quart through [environment]'s own per-quart cache.
     */
    private fun fillBiomes(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
    ) {
        for (quartY in 0 until WorldHeight.HEIGHT / 4) {
            val blockY = WorldHeight.MIN_Y + quartY * 4
            for (quartX in 0 until 4) {
                for (quartZ in 0 until 4) {
                    val biome =
                        environment.biomeAt(
                            chunk.chunkX * 16 + quartX * 4,
                            blockY,
                            chunk.chunkZ * 16 + quartZ * 4,
                        )
                    val id = Biomes.REGISTRY.idOf(Identifier.parse(biome)) ?: 0
                    chunk.setBiomeId(quartX, quartY, quartZ, id)
                }
            }
        }
    }
}

/**
 * // parity: how net.minecraft.world.level.levelgen.SurfaceRules.Context.SteepMaterialCondition
 * clamps its neighbor lookup to the chunk under construction (`Math.max(chunkBlockZ - 1, 0)` etc,
 * never crossing into a neighboring chunk).
 *
 * [biomeAt] also memoizes here, by quart (4-block) position: real vanilla's own `isBiome` check
 * re-samples the biome independently every time it is reached (there can be dozens in one
 * [SurfaceRuleData.overworld] sequence, all for the same block position), and [delegate]'s own
 * `biomeAt` is a real climate lookup (a brute-force scan over thousands of registered regions, see
 * [OverworldBiomeSource]'s own KDoc), not a cheap field read. This cache is safe as a plain
 * `MutableMap` specifically because one [ChunkScopedSurfaceEnvironment] is only ever used by the
 * single thread generating this one chunk, then discarded: no cross-thread sharing, unlike
 * [OverworldBiomeSource] itself (see its own KDoc for why a shared cache there was a real, measured
 * regression: unbounded copy-on-write growth turned a ~1s startup into 45s).
 */
private class ChunkScopedSurfaceEnvironment(
    private val delegate: SurfaceRuleEnvironment,
    private val heightmap: IntArray,
    private val cache: DensityCache,
) : SurfaceRuleEnvironment by delegate {
    private val biomeCache = HashMap<Long, String>()

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): String {
        val key =
            ((x shr 2).toLong() and 0x1FFFFFL) or
                (((y shr 2).toLong() and 0xFFFFFL) shl 21) or
                (((z shr 2).toLong() and 0x1FFFFFL) shl 41)
        biomeCache[key]?.let { return it }
        val biome = delegate.biomeAt(x, y, z, cache)
        biomeCache[key] = biome
        return biome
    }

    override fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int = heightmap[(x and 15) + (z and 15) * 16]

    override fun isSteep(
        x: Int,
        z: Int,
    ): Boolean {
        val localX = x and 15
        val localZ = z and 15
        val zNorth = maxOf(localZ - 1, 0)
        val zSouth = minOf(localZ + 1, 15)
        if (heightAt(localX, zSouth) >= heightAt(localX, zNorth) + 4) return true
        val xWest = maxOf(localX - 1, 0)
        val xEast = minOf(localX + 1, 15)
        return heightAt(xWest, localZ) >= heightAt(xEast, localZ) + 4
    }

    private fun heightAt(
        localX: Int,
        localZ: Int,
    ): Int = heightmap[localX + localZ * 16]
}
