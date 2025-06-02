package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * Real ocean-floor vegetation shared by the seven non-frozen ocean variants. Kelp age,
 * sea-pickle count/waterlogging and both tall-seagrass halves preserve their exact block states.
 *
 * Warm-ocean coral lives in [CoralDecoration]. Seagrass, kelp and sea pickles below are complete
 * translations of their feature bodies and placed-feature pipelines.
 */
public object AquaticDecoration {
    private val SEA_PICKLES: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val oceanFloor = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
            runPipeline(
                chunk,
                random,
                listOf(
                    RarityFilter(16),
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR),
                ),
            ) { origin -> placeSeaPickles(chunk, random, origin, oceanFloor, count = 20) }
        }

    public val OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 48, tallProbability = 0.3), kelp(noiseToCountRatio = 120))

    public val DEEP_OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 48, tallProbability = 0.8), kelp(noiseToCountRatio = 120))

    public val COLD_OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 32, tallProbability = 0.3), kelp(noiseToCountRatio = 120))

    public val DEEP_COLD_OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 40, tallProbability = 0.8), kelp(noiseToCountRatio = 120))

    public val LUKEWARM_OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 80, tallProbability = 0.3), kelp(noiseToCountRatio = 80))

    public val DEEP_LUKEWARM_OCEAN: BiomeVegetalDecoration =
        combine(seagrass(count = 80, tallProbability = 0.8), kelp(noiseToCountRatio = 80))

    public val RIVER: BiomeVegetalDecoration = seagrass(count = 48, tallProbability = 0.4)

    public val SWAMP: BiomeVegetalDecoration = seagrass(count = 64, tallProbability = 0.6)

    public val WARM_OCEAN: BiomeVegetalDecoration =
        combine(CoralDecoration, seagrass(count = 80, tallProbability = 0.3), SEA_PICKLES)

    private fun seagrass(
        count: Int,
        tallProbability: Double,
    ): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val oceanFloor = chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR)
            runPipeline(
                chunk,
                random,
                listOf(
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR),
                    CountPlacement(count),
                ),
            ) { origin -> placeSeagrass(chunk, random, origin, oceanFloor, tallProbability) }
        }

    private fun kelp(noiseToCountRatio: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            runPipeline(
                chunk,
                random,
                listOf(
                    NoiseBasedCountPlacement(noiseToCountRatio, noiseFactor = 80.0),
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.OCEAN_FLOOR),
                ),
            ) { origin -> placeKelp(chunk, random, origin) }
        }

    private fun placeSeagrass(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        oceanFloor: IntArray,
        tallProbability: Double,
    ) {
        val x = origin.x + random.nextInt(8) - random.nextInt(8)
        val z = origin.z + random.nextInt(8) - random.nextInt(8)
        val y = oceanFloor[(x and 15) + (z and 15) * 16]
        if (!chunk.isWater(x, y, z)) return

        val tall = random.nextDouble() < tallProbability
        if (!canAquaticPlantSurvive(chunk, x, y, z)) return
        if (tall) {
            if (!chunk.isWater(x, y + 1, z)) return
            chunk.setLocalBlock(x, y, z, Blocks.TALL_SEAGRASS.stateId)
            chunk.setLocalBlock(
                x,
                y + 1,
                z,
                Blocks.upperDoublePlantStateId(Blocks.TALL_SEAGRASS.stateId)
                    ?: error("missing tall seagrass upper half"),
            )
        } else {
            chunk.setLocalBlock(x, y, z, Blocks.SEAGRASS.stateId)
        }
    }

    private fun placeKelp(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        var y = origin.y
        if (!chunk.isWater(origin.x, y, origin.z)) return

        val height = 1 + random.nextInt(10)
        for (h in 0..height) {
            if (chunk.isWater(origin.x, y, origin.z) &&
                chunk.isWater(origin.x, y + 1, origin.z) &&
                canKelpSurvive(chunk, origin.x, y, origin.z)
            ) {
                if (h == height) {
                    chunk.setLocalBlock(origin.x, y, origin.z, Blocks.KELP.stateId + 20 + random.nextInt(4))
                    return
                }
                chunk.setLocalBlock(origin.x, y, origin.z, Blocks.KELP_PLANT.stateId)
                y++
            } else {
                if (h > 0 && canKelpSurvive(chunk, origin.x, y - 1, origin.z)) {
                    chunk.setLocalBlock(origin.x, y - 1, origin.z, Blocks.KELP.stateId + 20 + random.nextInt(4))
                }
                return
            }
        }
    }

    private fun placeSeaPickles(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        oceanFloor: IntArray,
        count: Int,
    ) {
        repeat(count) {
            val x = origin.x + random.nextInt(8) - random.nextInt(8)
            val z = origin.z + random.nextInt(8) - random.nextInt(8)
            val y = oceanFloor[(x and 15) + (z and 15) * 16]
            val stateId = Blocks.SEA_PICKLE.stateId + random.nextInt(4) * 2
            if (chunk.isWater(x, y, z) && canAquaticPlantSurvive(chunk, x, y, z)) {
                chunk.setLocalBlock(x, y, z, stateId)
            }
        }
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        place: (DecorationPos) -> Unit,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach(place)
    }

    private fun canAquaticPlantSurvive(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y <= WorldHeight.MIN_Y) return false
        val below = chunk.getLocalBlock(x, y - 1, z)
        return !Blocks.isAir(below) && below != Blocks.WATER.stateId
    }

    private fun canKelpSurvive(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (y <= WorldHeight.MIN_Y) return false
        val below = chunk.getLocalBlock(x, y - 1, z)
        return !Blocks.isAir(below) && below != Blocks.WATER.stateId && !Blocks.isKelpState(below)
    }

    private fun Chunk.isWater(
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Boolean = y in WorldHeight.MIN_Y..WorldHeight.MAX_Y && getLocalBlock(worldX, y, worldZ) == Blocks.WATER.stateId

    private fun Chunk.getLocalBlock(
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int = getBlockStateId(worldX and 15, y, worldZ and 15)

    private fun Chunk.setLocalBlock(
        worldX: Int,
        y: Int,
        worldZ: Int,
        stateId: Int,
    ) {
        if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) setBlockStateId(worldX and 15, y, worldZ and 15, stateId)
    }
}
