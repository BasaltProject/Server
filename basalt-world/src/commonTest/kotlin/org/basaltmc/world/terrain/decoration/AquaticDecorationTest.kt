package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class AquaticDecorationTest {
    @Test
    fun oceanPlacesPlantsOnTheOceanFloorWithoutRemovingTheWaterSurface() {
        val chunk = oceanChunk()
        AquaticDecoration.OCEAN.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        var aquaticPlants = 0
        for (section in 0 until chunk.sectionCount) {
            aquaticPlants +=
                chunk.section(section).blockStateIds().count {
                    it == Blocks.SEAGRASS.stateId ||
                        it == Blocks.TALL_SEAGRASS.stateId ||
                        it == Blocks.KELP.stateId ||
                        it == Blocks.KELP_PLANT.stateId
                }
        }
        assertTrue(aquaticPlants > 0)
        assertEquals(63, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING).max())
        assertEquals(41, chunk.heightmap(Chunk.HeightmapType.OCEAN_FLOOR).max())
    }

    @Test
    fun warmOceanPlacesCoralBlocksBelowTheWaterSurface() {
        val chunkX =
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
        assertEquals(63, chunk.heightmap(Chunk.HeightmapType.MOTION_BLOCKING).max())
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
}
