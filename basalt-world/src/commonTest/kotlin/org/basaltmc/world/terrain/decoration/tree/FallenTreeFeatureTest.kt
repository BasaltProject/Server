package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class FallenTreeFeatureTest {
    @Test
    fun oakPlacesAStumpAndSidewaysLogOnGround() {
        val chunk = flatChunk()
        FallenTreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 8, 63, 8, FallenTreeConfigs.OAK)

        assertEquals(Blocks.OAK_LOG.stateId, chunk.getBlockStateId(8, 63, 8))
        val sidewaysStates = setOf(Blocks.OAK_LOG.stateId - 1, Blocks.OAK_LOG.stateId + 1)
        assertTrue(count(chunk, sidewaysStates) in 2..5)
    }

    @Test
    fun everySpeciesConfigurationPlacesItsOwnLogFamily() {
        val configs =
            listOf(
                FallenTreeConfigs.BIRCH,
                FallenTreeConfigs.SUPER_BIRCH,
                FallenTreeConfigs.SPRUCE,
                FallenTreeConfigs.JUNGLE,
            )
        for ((index, config) in configs.withIndex()) {
            val chunk = flatChunk()
            FallenTreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(index.toLong() + 9)), 8, 63, 8, config)
            assertEquals(config.logStateId, chunk.getBlockStateId(8, 63, 8))
            assertTrue(count(chunk, setOf(config.logStateId - 1, config.logStateId + 1)) > 0)
        }
    }

    @Test
    fun oakAndJungleStumpsUseTheFourExactVineFaceStates() {
        val defaultVine = stateId("vine")
        val expected =
            listOf(
                Triple(7 to 8, defaultVine - 16, "west placement faces east"),
                Triple(9 to 8, defaultVine - 1, "east placement faces west"),
                Triple(8 to 7, defaultVine - 4, "north placement faces south"),
                Triple(8 to 9, defaultVine - 8, "south placement faces north"),
            )
        var placed = 0
        for (seed in 0L..15L) {
            val chunk = flatChunk()
            FallenTreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 8, 63, 8, FallenTreeConfigs.OAK)
            for ((position, stateId, message) in expected) {
                val actual = chunk.getBlockStateId(position.first, 63, position.second)
                assertTrue(actual == Blocks.AIR.stateId || actual == stateId, message)
                if (actual == stateId) placed++
            }
        }
        assertTrue(placed > 0)
    }

    @Test
    fun fallenLogsReceiveWeightedMushroomsAboveTheirLogs() {
        val mushrooms = setOf(stateId("red_mushroom"), stateId("brown_mushroom"))
        val sidewaysBirch =
            setOf(
                FallenTreeConfigs.SUPER_BIRCH.logStateId - 1,
                FallenTreeConfigs.SUPER_BIRCH.logStateId + 1,
            )
        var placed = 0
        for (seed in 0L..31L) {
            val chunk = flatChunk()
            FallenTreeFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                8,
                63,
                8,
                FallenTreeConfigs.SUPER_BIRCH,
            )
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 64..66) {
                        if (chunk.getBlockStateId(x, y, z) in mushrooms) {
                            assertTrue(chunk.getBlockStateId(x, y - 1, z) in sidewaysBirch)
                            placed++
                        }
                    }
                }
            }
        }
        assertTrue(placed > 0)
    }

    private fun flatChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun count(
        chunk: Chunk,
        states: Set<Int>,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60..66) if (chunk.getBlockStateId(x, y, z) in states) result++
            }
        }
        return result
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
