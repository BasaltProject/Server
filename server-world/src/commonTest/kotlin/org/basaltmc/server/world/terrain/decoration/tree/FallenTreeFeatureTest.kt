package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
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

    @Test
    fun fallenLogsReplaceVegetationFromTheTreeTag() {
        val chunk = flatChunk()
        val shortGrass = stateId("short_grass")
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 63, z, shortGrass)
        }

        FallenTreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 8, 63, 8, FallenTreeConfigs.OAK)

        val sidewaysStates = setOf(Blocks.OAK_LOG.stateId - 1, Blocks.OAK_LOG.stateId + 1)
        assertTrue(count(chunk, sidewaysStates) > 0)
    }

    @Test
    fun fallenLogsRequireASturdyUpperGroundFace() {
        val bottomSlab = stateId("stone_slab")
        val topSlab = bottomSlab - 2
        val unsupported = slabGround(bottomSlab)
        FallenTreeFeature.place(
            unsupported,
            WorldgenRandom(XoroshiroRandom(42L)),
            8,
            63,
            8,
            FallenTreeConfigs.OAK,
        )
        val sidewaysStates = setOf(Blocks.OAK_LOG.stateId - 1, Blocks.OAK_LOG.stateId + 1)
        assertEquals(0, count(unsupported, sidewaysStates))

        val supported = slabGround(topSlab)
        FallenTreeFeature.place(
            supported,
            WorldgenRandom(XoroshiroRandom(42L)),
            8,
            63,
            8,
            FallenTreeConfigs.OAK,
        )
        assertTrue(count(supported, sidewaysStates) > 0)
    }

    @Test
    fun fallenLogsCrossTheBorderInsteadOfWrapping() {
        var foundExternalLog = false
        for (seed in 0L..64L) {
            val chunk = flatChunk()
            for (x in 16..30) chunk.setDecorationBlockStateId(x, 62, 8, Blocks.GRASS_BLOCK.stateId)
            FallenTreeFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(seed)),
                15,
                63,
                8,
                FallenTreeConfigs.SUPER_BIRCH,
            )
            val sidewaysBirch =
                setOf(FallenTreeConfigs.SUPER_BIRCH.logStateId - 1, FallenTreeConfigs.SUPER_BIRCH.logStateId + 1)
            val writes = chunk.takeGeneratedBlockWrites()
            if (writes.any { it.x >= 16 && it.stateId in sidewaysBirch }) {
                foundExternalLog = true
                for (y in 63..66) assertTrue(chunk.getBlockStateId(0, y, 8) !in sidewaysBirch)
                break
            }
        }
        assertTrue(foundExternalLog, "expected a fallen birch directed east across the border")
    }

    private fun flatChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun slabGround(stateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, stateId)
            }
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
