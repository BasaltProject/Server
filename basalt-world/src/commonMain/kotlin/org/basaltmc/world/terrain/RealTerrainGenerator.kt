package org.basaltmc.world.terrain

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.Marker
import org.basaltmc.world.terrain.carver.OverworldCarvers
import org.basaltmc.world.terrain.decoration.BiomeDecoration
import org.basaltmc.world.terrain.decoration.OverworldOreDecoration
import org.basaltmc.world.terrain.decoration.SurfaceFreezingDecoration
import org.basaltmc.world.terrain.surface.SurfaceBuilder
import org.basaltmc.world.terrain.surface.SurfaceRuleData
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment

/**
 * Chunk shape from an [OverworldNoiseRouter]'s `finalDensity` through [DensityChunkSampler], then
 * surface rules, structures and [BiomeDecoration], in that order.
 * // parity: ChunkGenerator.applyBiomeDecoration, whose decoration seed is the plain world seed.
 *
 * Build the router once per world: it is dozens of nested nodes. [environment] gets the chunk's
 * heightmap so surface rules read an array instead of re-sampling density, 80x fewer evaluations.
 *
 * Gap: decoration only for the biomes ported so far.
 */
public object RealTerrainGenerator {
    private const val CELL_WIDTH = 4
    private const val CELL_HEIGHT = 8

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
                villageGenerator?.finalDensityForChunk(chunkX, chunkZ, seed) ?: router.finalDensity,
                chunkX,
                chunkZ,
                WorldHeight.MIN_Y,
                WorldHeight.HEIGHT,
                CELL_WIDTH,
                CELL_HEIGHT,
                cache,
            )
        val aquifer = OverworldAquifer(router, seed, chunkX, chunkZ, WorldHeight.MIN_Y, WorldHeight.HEIGHT, cache)
        val oreVeins = OverworldOreVeinifier(router, seed, cache)
        GenerationMetrics.measure(GenerationMetrics.Phase.DENSITY_GRID) { sampler.prepare() }
        GenerationMetrics.measure(GenerationMetrics.Phase.TERRAIN_FILL) {
            // Cell by cell rather than column by column, so a cell of open sky (all eight corners
            // non-positive and entirely at or above sea level, which is most of a chunk's height)
            // is skipped whole: those blocks are air, and air is already what the chunk holds.
            // Within a cell the order is y, then z, then x, matching ChunkSection's own
            // (y shl 8) or (z shl 4) or x layout so writes run forwards through its array.
            for (cellY in 0 until sampler.verticalCells) {
                val cellMinY = WorldHeight.MIN_Y + cellY * CELL_HEIGHT
                val cellMaxY = cellMinY + CELL_HEIGHT - 1
                for (cellX in 0 until sampler.horizontalCells) {
                    for (cellZ in 0 until sampler.horizontalCells) {
                        for (y in cellMinY..cellMaxY) {
                            for (z in cellZ * CELL_WIDTH until (cellZ + 1) * CELL_WIDTH) {
                                for (x in cellX * CELL_WIDTH until (cellX + 1) * CELL_WIDTH) {
                                    val density = sampler.sample(x, y, z)
                                    val material =
                                        aquifer.computeSubstance(
                                            chunkX * 16 + x,
                                            y,
                                            chunkZ * 16 + z,
                                            density,
                                        )
                                    if (material == null) {
                                        chunk.setBlockStateId(
                                            x,
                                            y,
                                            z,
                                            oreVeins.compute(chunkX * 16 + x, y, chunkZ * 16 + z)?.stateId
                                                ?: Blocks.STONE.stateId,
                                        )
                                    } else if (material != Blocks.AIR) {
                                        chunk.setBlockStateId(x, y, z, material.stateId)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        val heightmap = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
        val chunkEnvironment = ChunkScopedSurfaceEnvironment(environment, heightmap, cache)
        val surfaceRule = SurfaceRuleData.overworld()
        GenerationMetrics.measure(GenerationMetrics.Phase.SURFACE_RULES) {
            SurfaceBuilder.buildSurface(
                chunk,
                chunkEnvironment,
                surfaceRule,
            )
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.CARVERS) {
            OverworldCarvers.carve(chunk, seed, aquifer, chunkEnvironment, surfaceRule)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.STRUCTURES) {
            villageGenerator?.generateInto(chunk, chunkX, chunkZ, seed)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.DECORATION) {
            OverworldOreDecoration.decorate(chunk, seed, chunkEnvironment)
            val decorationRandom = WorldgenRandom(XoroshiroRandom(seed))
            decorationRandom.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
            val centerY = heightmap[8 + 8 * 16]
            val biomeId = Identifier.parse(chunkEnvironment.biomeAt(chunkX * 16 + 8, centerY, chunkZ * 16 + 8))
            BiomeDecoration.forBiome(biomeId)?.decorate(chunk, decorationRandom)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.SURFACE_FREEZE) {
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
 * // parity: SurfaceRules.Context.SteepMaterialCondition clamps its neighbour lookup to the chunk
 * under construction rather than crossing into the next one.
 *
 * [biomeAt] memoizes by quart position, because vanilla's `isBiome` re-samples for every check in
 * a rule sequence and [delegate]'s lookup is a scan over thousands of climate regions. A plain
 * `MutableMap` is safe here only because one instance serves one chunk on one thread and is then
 * discarded, unlike the shared [OverworldBiomeSource].
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
