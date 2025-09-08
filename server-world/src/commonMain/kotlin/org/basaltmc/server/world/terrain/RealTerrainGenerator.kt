package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.GenerationMetrics
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.Marker
import org.basaltmc.server.world.terrain.carver.OverworldCarvers
import org.basaltmc.server.world.terrain.decoration.CaveLocalModificationDecoration
import org.basaltmc.server.world.terrain.decoration.CaveSpeleothemDecoration
import org.basaltmc.server.world.terrain.decoration.DesertWellDecoration
import org.basaltmc.server.world.terrain.decoration.ForestRockDecoration
import org.basaltmc.server.world.terrain.decoration.FrozenOceanDecoration
import org.basaltmc.server.world.terrain.decoration.IceSpikesDecoration
import org.basaltmc.server.world.terrain.decoration.OverworldLakeDecoration
import org.basaltmc.server.world.terrain.decoration.OverworldOreDecoration
import org.basaltmc.server.world.terrain.decoration.OverworldSpringDecoration
import org.basaltmc.server.world.terrain.decoration.OverworldUndergroundStructures
import org.basaltmc.server.world.terrain.decoration.OverworldVegetalDecoration
import org.basaltmc.server.world.terrain.decoration.SculkDecoration
import org.basaltmc.server.world.terrain.decoration.SurfaceFreezingDecoration
import org.basaltmc.server.world.terrain.surface.ChunkBiomeEnvironment
import org.basaltmc.server.world.terrain.surface.SurfaceBuilder
import org.basaltmc.server.world.terrain.surface.SurfaceRuleData
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * Chunk shape from an [OverworldNoiseRouter]'s `finalDensity` through [DensityChunkSampler], then
 * surface rules, structures and independently seeded decoration dispatchers, in that order.
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

    /** A chunk carrying everything a configured feature may read before decoration runs. */
    public class GeneratedTerrain internal constructor(
        public val chunk: Chunk,
        internal val environment: SurfaceRuleEnvironment,
    )

    public fun generate(
        router: OverworldNoiseRouter,
        environment: SurfaceRuleEnvironment,
        chunkX: Int,
        chunkZ: Int,
        seed: Long = 0L,
        villageGenerator: VillageStructureGenerator? = null,
        neighbours: NeighbourTerrainCache? = null,
        mineshaftGenerator: MineshaftStructureGenerator? = null,
        ruinedPortalGenerator: RuinedPortalStructureGenerator? = null,
        ancientCityGenerator: AncientCityStructureGenerator? = null,
    ): Chunk {
        val terrain =
            generateTerrain(
                router,
                environment,
                chunkX,
                chunkZ,
                seed,
                villageGenerator,
                mineshaftGenerator,
                ruinedPortalGenerator,
                ancientCityGenerator,
            )
        decorate(terrain, seed, neighbours?.around(chunkX, chunkZ), ancientCityGenerator)
        return terrain.chunk
    }

    /**
     * Everything a chunk holds before its own decoration: noise shape, surface rules, carvers and
     * structures. A neighbour is generated only this far when a feature needs to read across the
     * chunk edge, so reading one never recurses back into decoration.
     */
    public fun generateTerrain(
        router: OverworldNoiseRouter,
        environment: SurfaceRuleEnvironment,
        chunkX: Int,
        chunkZ: Int,
        seed: Long = 0L,
        villageGenerator: VillageStructureGenerator? = null,
        mineshaftGenerator: MineshaftStructureGenerator? = null,
        ruinedPortalGenerator: RuinedPortalStructureGenerator? = null,
        ancientCityGenerator: AncientCityStructureGenerator? = null,
    ): GeneratedTerrain {
        val chunk = Chunk(chunkX, chunkZ)
        // One cache for the whole chunk, shared by terrain shape, surface rules and biome filling:
        // they all walk the same router subtrees, so a cache per phase would throw away most of it.
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                (villageGenerator?.finalDensityForChunk(chunkX, chunkZ, seed) ?: router.finalDensity)
                    .let { ancientCityGenerator?.beardified(it, chunkX, chunkZ, seed) ?: it },
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
        GenerationMetrics.measure(GenerationMetrics.Phase.TERRAIN_FILL) {
            // Cell by cell rather than column by column, so a cell's eight interpolation corners
            // are computed once and every block inside pays only the lerp (see DensityCache).
            // Cells are never skipped wholesale: interpolation is marker-scoped, nodes outside the
            // markers evaluate per block, so corner signs alone cannot prove a cell empty.
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
        val chunkEnvironment =
            ChunkScopedSurfaceEnvironment(ChunkBiomeEnvironment(environment, cache), heightmap, chunkX, chunkZ)
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
            // Step 3, underground_structures, which is before every decoration step.
            mineshaftGenerator?.generateInto(chunk, seed) { x, z ->
                chunk.heightAt(x and 15, z and 15, Chunk.HeightmapType.OCEAN_FLOOR)
            }
            // Step 4, surface_structures, alongside the villages above.
            ruinedPortalGenerator?.generateInto(chunk, seed)
        }
        return GeneratedTerrain(chunk, chunkEnvironment)
    }

    /** The decoration steps of one already-generated [terrain], in their official order. */
    public fun decorate(
        terrain: GeneratedTerrain,
        seed: Long = 0L,
        neighbours: NeighbourTerrain? = null,
        ancientCityGenerator: AncientCityStructureGenerator? = null,
    ) {
        val chunk = terrain.chunk
        val chunkEnvironment = terrain.environment
        GenerationMetrics.measure(GenerationMetrics.Phase.DECORATION) {
            // One reader for the whole phase. A feature body that reaches past the chunk edge asks
            // the chunk, and the chunk asks this: without it the outside reads as air, which makes
            // a lake preflight reject itself and a grass patch find no ground to stand on.
            chunk.withExternalDecorationBlockState(
                reader = { x, y, z -> neighbours?.blockStateIdAt(x, y, z) },
            ) {
                OverworldLakeDecoration.decorate(chunk, seed, chunkEnvironment)
                FrozenOceanDecoration.decorateLocalModifications(chunk, seed, chunkEnvironment)
                CaveLocalModificationDecoration.decorate(chunk, seed, chunkEnvironment)
                ForestRockDecoration.decorate(chunk, seed, chunkEnvironment)
                IceSpikesDecoration.decorate(chunk, seed, chunkEnvironment)
                DesertWellDecoration.decorate(chunk, seed, chunkEnvironment)
                OverworldUndergroundStructures.decorate(chunk, seed, chunkEnvironment)
                FrozenOceanDecoration.decorateSurfaceStructures(chunk, seed, chunkEnvironment)
                // Ore and stone blobs are spheres around their origin, so most cross a chunk edge. The
                // part outside is deferred, and its guard has to be the block actually out there.
                OverworldOreDecoration.decorate(
                    chunk,
                    seed,
                    chunkEnvironment,
                    // A blob's preflight asks for the height of every column its box covers, and
                    // most of those columns belong to a neighbour.
                    externalSurfaceHeight = { x, z -> neighbours?.oceanFloorHeightAt(x, z) },
                    externalBlockState = { x, y, z -> neighbours?.blockStateIdAt(x, y, z) },
                )
                // Step 7 opens with its structures, and the ancient city is the first of the
                // three, so the sculk features below decorate a chunk that already has the city in
                // it rather than the bare deepslate it would otherwise see.
                ancientCityGenerator?.generateInto(chunk, chunk.chunkX, chunk.chunkZ, seed)
                SculkDecoration.decorate(chunk, seed, chunkEnvironment)
                CaveSpeleothemDecoration.decorate(chunk, seed, chunkEnvironment)
                OverworldOreDecoration.decorateInfested(
                    chunk,
                    seed,
                    chunkEnvironment,
                    externalBlockState = { x, y, z -> neighbours?.blockStateIdAt(x, y, z) },
                )
                OverworldSpringDecoration.decorate(chunk, seed, chunkEnvironment)
                OverworldVegetalDecoration.decorate(chunk, seed, chunkEnvironment)
            }
        }
        // The water over a magma block or soul sand carries its column in the saved chunk, so this
        // runs once decoration has put both of them down.
        BubbleColumnFormation.formInto(chunk)
        GenerationMetrics.measure(GenerationMetrics.Phase.SURFACE_FREEZE) {
            SurfaceFreezingDecoration.decorate(chunk, chunkEnvironment)
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.BIOME_FILL) { fillBiomes(chunk, chunkEnvironment) }
        if (GenerationMetrics.enabled) GenerationMetrics.countChunk()
    }

    /**
     * Fills every quart cell of the chunk with its real biome network ID, so the wire palette
     * ([org.basaltmc.server.world.ChunkDataCodec]) carries the actual biome layout the client needs for
     * grass/water/fog colors. Sampled once per quart through [environment]'s own per-quart cache.
     */
    private fun fillBiomes(
        chunk: Chunk,
        environment: SurfaceRuleEnvironment,
    ) {
        // A chunk holds a handful of distinct biomes, so the Identifier parse and the registry
        // lookup are paid once per name instead of once per each of the 1536 quarts.
        val idByName = HashMap<String, Int>()
        for (quartY in 0 until WorldHeight.HEIGHT / 4) {
            val blockY = WorldHeight.MIN_Y + quartY * 4
            for (quartX in 0 until 4) {
                for (quartZ in 0 until 4) {
                    val biome =
                        environment.noiseBiomeAt(
                            chunk.chunkX * 4 + quartX,
                            blockY shr 2,
                            chunk.chunkZ * 4 + quartZ,
                        )
                    val id = idByName.getOrPut(biome) { Biomes.REGISTRY.idOf(Identifier.parse(biome)) ?: 0 }
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
 * Biome zoom and raw-quart caching live in [ChunkBiomeEnvironment]. This final wrapper replaces
 * only the surface height and steepness queries that need the chunk currently under construction.
 */
private class ChunkScopedSurfaceEnvironment(
    private val delegate: SurfaceRuleEnvironment,
    private val heightmap: IntArray,
    private val chunkX: Int,
    private val chunkZ: Int,
) : SurfaceRuleEnvironment by delegate {
    override fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int =
        if (x shr 4 == chunkX && z shr 4 == chunkZ) {
            heightmap[(x and 15) + (z and 15) * 16]
        } else {
            delegate.estimatedSurfaceHeight(x, z)
        }

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
