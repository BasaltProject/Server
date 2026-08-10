package org.basaltmc.world.terrain

import org.basaltmc.registry.Biome
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.Marker
import org.basaltmc.world.terrain.carver.NetherCaveCarver
import org.basaltmc.world.terrain.decoration.nether.NetherLocalDecoration
import org.basaltmc.world.terrain.decoration.nether.NetherSurfaceStructuresDecoration
import org.basaltmc.world.terrain.decoration.nether.NetherUndergroundDecoration
import org.basaltmc.world.terrain.decoration.nether.NetherVegetalDecoration
import org.basaltmc.world.terrain.surface.SurfaceBuilder
import org.basaltmc.world.terrain.surface.SurfaceRuleData
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment

/** Base terrain and biome fill for vanilla's Nether and End noise settings. */
public object DimensionTerrainGenerator {
    /** // parity: `noise_settings/nether.json` base shape, block/fluid defaults and cell sizes. */
    public fun nether(
        router: NetherNoiseRouter,
        biomeSource: NetherBiomeSource,
        chunkX: Int,
        chunkZ: Int,
        surfaceEnvironment: SurfaceRuleEnvironment? = null,
        seed: Long = 0L,
    ): Chunk =
        generate(
            finalDensity = router.finalDensity,
            verticalBounds = ChunkHeight.NETHER,
            noiseMinY = 0,
            noiseHeight = 128,
            cellWidth = 4,
            cellHeight = 8,
            defaultBlock = Blocks.NETHERRACK.stateId,
            defaultFluid = Blocks.LAVA.stateId,
            fluidLevel = 32,
            chunkX = chunkX,
            chunkZ = chunkZ,
            surfacePass = { chunk ->
                if (surfaceEnvironment != null) {
                    SurfaceBuilder.buildSurface(
                        chunk,
                        surfaceEnvironment,
                        SurfaceRuleData.nether(),
                        Blocks.NETHERRACK.stateId,
                        generationMinY = 0,
                        generationHeight = 128,
                    )
                }
            },
            carverPass = { chunk -> NetherCaveCarver.carve(chunk, seed) },
            decorationPass = { chunk ->
                NetherLocalDecoration.decorate(chunk, seed, biomeSource)
                NetherSurfaceStructuresDecoration.decorate(chunk, seed, biomeSource)
                NetherUndergroundDecoration.decorate(chunk, seed, biomeSource)
                NetherVegetalDecoration.decorate(chunk, seed, biomeSource)
            },
        ) { blockX, blockY, blockZ, cache -> biomeSource.biomeAt(blockX, blockY, blockZ, cache) }

    /** // parity: `noise_settings/end.json` base shape, block default and cell sizes. */
    public fun end(
        router: EndNoiseRouter,
        biomeSource: TheEndBiomeSource,
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        generate(
            finalDensity = router.finalDensity,
            verticalBounds = ChunkHeight.END,
            noiseMinY = 0,
            noiseHeight = 128,
            cellWidth = 8,
            cellHeight = 4,
            defaultBlock = Blocks.END_STONE.stateId,
            defaultFluid = Blocks.AIR.stateId,
            fluidLevel = 0,
            chunkX = chunkX,
            chunkZ = chunkZ,
        ) { blockX, blockY, blockZ, _ -> biomeSource.biomeAtQuart(blockX shr 2, blockY shr 2, blockZ shr 2) }

    private fun generate(
        finalDensity: DensityFunction,
        verticalBounds: ChunkHeight,
        noiseMinY: Int,
        noiseHeight: Int,
        cellWidth: Int,
        cellHeight: Int,
        defaultBlock: Int,
        defaultFluid: Int,
        fluidLevel: Int,
        chunkX: Int,
        chunkZ: Int,
        surfacePass: (Chunk) -> Unit = {},
        carverPass: (Chunk) -> Unit = {},
        structurePass: (Chunk) -> Unit = {},
        decorationPass: (Chunk) -> Unit = {},
        biomeAt: (blockX: Int, blockY: Int, blockZ: Int, cache: DensityCache) -> Biome,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ, verticalBounds)
        val cache = DensityCache(chunkX, chunkZ, Marker.slotCount)
        val sampler =
            DensityChunkSampler(
                finalDensity,
                chunkX,
                chunkZ,
                noiseMinY,
                noiseHeight,
                cellWidth,
                cellHeight,
                cache,
            )
        GenerationMetrics.measure(GenerationMetrics.Phase.DENSITY_GRID) { sampler.prepare() }
        GenerationMetrics.measure(GenerationMetrics.Phase.TERRAIN_FILL) {
            for (cellY in 0 until sampler.verticalCells) {
                val cellMinY = noiseMinY + cellY * cellHeight
                val cellMaxY = cellMinY + cellHeight - 1
                for (cellX in 0 until sampler.horizontalCells) {
                    for (cellZ in 0 until sampler.horizontalCells) {
                        if (
                            cellMinY >= fluidLevel &&
                            sampler.cellIsAllNonPositive(cellX, cellY, cellZ)
                        ) {
                            continue
                        }
                        for (y in cellMinY..cellMaxY) {
                            for (z in cellZ * cellWidth until (cellZ + 1) * cellWidth) {
                                for (x in cellX * cellWidth until (cellX + 1) * cellWidth) {
                                    val stateId =
                                        when {
                                            sampler.sample(x, y, z) > 0.0 -> defaultBlock
                                            y < fluidLevel -> defaultFluid
                                            else -> Blocks.AIR.stateId
                                        }
                                    if (!Blocks.isAir(stateId)) chunk.setBlockStateId(x, y, z, stateId)
                                }
                            }
                        }
                    }
                }
            }
        }
        GenerationMetrics.measure(GenerationMetrics.Phase.SURFACE_RULES) { surfacePass(chunk) }
        GenerationMetrics.measure(GenerationMetrics.Phase.CARVERS) { carverPass(chunk) }
        GenerationMetrics.measure(GenerationMetrics.Phase.STRUCTURES) { structurePass(chunk) }
        GenerationMetrics.measure(GenerationMetrics.Phase.DECORATION) { decorationPass(chunk) }
        GenerationMetrics.measure(GenerationMetrics.Phase.BIOME_FILL) {
            for (quartY in 0 until verticalBounds.height / 4) {
                val blockY = verticalBounds.minY + quartY * 4
                for (quartX in 0 until 4) {
                    for (quartZ in 0 until 4) {
                        val biome =
                            biomeAt(
                                chunkX * 16 + quartX * 4,
                                blockY,
                                chunkZ * 16 + quartZ * 4,
                                cache,
                            )
                        val id = Biomes.REGISTRY.idOf(biome.id) ?: error("unregistered biome ${biome.id}")
                        chunk.setBiomeId(quartX, quartY, quartZ, id)
                    }
                }
            }
        }
        if (GenerationMetrics.enabled) GenerationMetrics.countChunk()
        return chunk
    }
}
