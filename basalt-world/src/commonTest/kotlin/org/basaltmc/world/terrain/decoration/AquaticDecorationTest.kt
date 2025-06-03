package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class AquaticDecorationTest {
    @Test
    fun oceanPlacesPlantsOnTheOceanFloorWithoutRemovingTheWaterSurface() {
        val chunkX =
            (-50..50).first { candidate ->
                val empty = Chunk(candidate, 0)
                NoiseBasedCountPlacement(120, 80.0)
                    .getPositions(
                        PlacementContext(empty),
                        WorldgenRandom(XoroshiroRandom(0L)),
                        DecorationPos(candidate * 16, 0, 0),
                    ).any()
            }
        val chunk = oceanChunk(chunkX, 0)
        AquaticDecoration.DEEP_OCEAN.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        var aquaticPlants = 0
        val kelpTopStates = mutableListOf<Int>()
        for (section in 0 until chunk.sectionCount) {
            for (stateId in chunk.section(section).blockStateIds()) {
                if (
                    stateId == Blocks.SEAGRASS.stateId ||
                    Blocks.isDoublePlantState(stateId) ||
                    Blocks.isKelpState(stateId) ||
                    stateId == Blocks.KELP_PLANT.stateId
                ) {
                    aquaticPlants++
                }
                if (Blocks.isKelpState(stateId)) kelpTopStates += stateId
            }
        }
        assertTrue(aquaticPlants > 0, "expected aquatic plants")
        assertTrue(kelpTopStates.isNotEmpty(), "expected at least one kelp top")
        assertTrue(
            kelpTopStates.all { stateId ->
                Blocks
                    .byStateId(stateId)
                    ?.properties
                    ?.get("age")
                    ?.toIntOrNull()
                    ?.let { it in 20..23 } == true
            },
            "expected kelp top ages 20 through 23",
        )

        var tallSeagrassPairs = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 41 until 62) {
                    if (
                        chunk.getBlockStateId(x, y, z) == Blocks.TALL_SEAGRASS.stateId &&
                        chunk.getBlockStateId(x, y + 1, z) == Blocks.TALL_SEAGRASS.stateId - 1
                    ) {
                        tallSeagrassPairs++
                    }
                }
            }
        }
        assertTrue(tallSeagrassPairs > 0, "expected a lower and upper tall-seagrass pair")
        assertEquals(63, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING).max())
        assertEquals(41, chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR).max())
    }

    @Test
    fun warmOceanPlacesCoralBlocksBelowTheWaterSurface() {
        val chunkX = coralChunkX()
        val chunk = oceanChunk(chunkX, 0)
        CoralDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(9876L)))

        val coralBlocks =
            listOf("tube", "brain", "bubble", "fire", "horn")
                .mapNotNull { name ->
                    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("${name}_coral_block"))?.stateId
                }.toSet()
        val placed =
            (0 until chunk.sectionCount).sumOf { section ->
                chunk.section(section).blockStateIds().count { it in coralBlocks }
            }
        assertTrue(placed > 0)

        val coralPlantStates =
            (0 until chunk.sectionCount)
                .flatMap { section -> chunk.section(section).blockStateIds().filter(Blocks::isCoralPlantState) }
        assertTrue(coralPlantStates.isNotEmpty())
        assertTrue(coralPlantStates.all { Blocks.byStateId(it)?.properties?.get("waterlogged") == "true" })
        val wallCoralStates =
            coralPlantStates.filter { stateId ->
                Blocks
                    .byStateId(stateId)
                    ?.block
                    ?.path
                    ?.endsWith("_coral_wall_fan") == true
            }
        assertTrue(wallCoralStates.isNotEmpty())
        assertTrue(
            wallCoralStates.all { stateId ->
                Blocks.byStateId(stateId)?.properties?.get("facing") in setOf("north", "east", "south", "west")
            },
        )
        assertEquals(63, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING).max())
    }

    @Test
    fun coralFeatureSeaPicklesUseExactWaterloggedCountStates() {
        val states = mutableSetOf<Int>()
        for (seed in 0L..128L) {
            val chunk = oceanChunk(coralChunkX(), 0)
            CoralDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            for (section in 0 until chunk.sectionCount) {
                for (stateId in chunk.section(section).blockStateIds()) {
                    if (Blocks.isSeaPickleState(stateId)) states += stateId
                }
            }
            if (states.isNotEmpty()) break
        }

        assertTrue(states.isNotEmpty())
        assertTrue(
            states.all { stateId ->
                val properties = Blocks.byStateId(stateId)?.properties ?: return@all false
                properties["pickles"]?.toIntOrNull()?.let { it in 1..4 } == true &&
                    properties["waterlogged"] == "true"
            },
        )
    }

    @Test
    fun warmOceanSeaPicklesUseWaterloggedCountStates() {
        val states = mutableSetOf<Int>()
        for (seed in 0L..128L) {
            val chunk = oceanChunk()
            AquaticDecoration.WARM_OCEAN.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            for (section in 0 until chunk.sectionCount) {
                for (stateId in chunk.section(section).blockStateIds()) {
                    if (Blocks.isSeaPickleState(stateId)) states += stateId
                }
            }
            if (states.isNotEmpty()) break
        }

        assertTrue(states.isNotEmpty())
        assertTrue(
            states.all { stateId ->
                val properties = Blocks.byStateId(stateId)?.properties ?: return@all false
                properties["pickles"]?.toIntOrNull()?.let { it in 1..4 } == true &&
                    properties["waterlogged"] == "true"
            },
        )
    }

    private fun oceanChunk(
        chunkX: Int = 0,
        chunkZ: Int = 0,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..40) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 41 until 63) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun coralChunkX(): Int =
        (-20..20).first { candidate ->
            val empty = Chunk(candidate, 0)
            val positions =
                NoiseBasedCountPlacement(20, 400.0).getPositions(
                    PlacementContext(empty),
                    WorldgenRandom(XoroshiroRandom(0L)),
                    DecorationPos(candidate * 16, 0, 0),
                )
            positions.any()
        }
}
