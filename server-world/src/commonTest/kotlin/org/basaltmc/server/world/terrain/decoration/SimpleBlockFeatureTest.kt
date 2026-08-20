package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

class SimpleBlockFeatureTest {
    @Test
    fun stateProviderRunsBeforeFailedSurvivalCheck() {
        val actual = WorldgenRandom(XoroshiroRandom(91L))
        val expected = WorldgenRandom(XoroshiroRandom(91L))
        val feature =
            SimpleBlockFeature({ random, _ ->
                random.nextInt(16)
                Blocks.GRASS_BLOCK.stateId
            })

        feature.place(Chunk(0, 0), actual, DecorationPos(0, 63, 0))
        expected.nextInt(16)

        assertEquals(expected.nextLong(), actual.nextLong())
    }

    @Test
    fun sequentialExternalWritesRemainAtTheirWorldCoordinates() {
        val chunk = Chunk(0, 0)
        chunk.setDecorationBlockStateId(16, 62, 4, Blocks.GRASS_BLOCK.stateId)

        assertTrue(
            SimpleBlockFeature({ _, _ -> Blocks.OAK_LOG.stateId })
                .place(chunk, WorldgenRandom(XoroshiroRandom(1L)), DecorationPos(16, 63, 4)),
        )

        val writes = chunk.takeGeneratedBlockWrites()
        assertEquals(Blocks.GRASS_BLOCK.stateId, writes.single { it.y == 62 }.stateId)
        assertEquals(Blocks.OAK_LOG.stateId, writes.single { it.y == 63 }.stateId)
        assertTrue(writes.all { it.x == 16 && it.z == 4 })
    }

    @Test
    fun flowersUseSupportsVegetationInsteadOfAnySolidGround() {
        val flowerStateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dandelion"))?.stateId
                ?: error("missing dandelion")
        val feature = SimpleBlockFeature({ _, _ -> flowerStateId })
        val random = WorldgenRandom(XoroshiroRandom(1L))
        val stone = Chunk(0, 0).also { it.setBlockStateId(8, 39, 8, Blocks.STONE.stateId) }
        val snowyGrass = Chunk(0, 0).also { it.setBlockStateId(8, 39, 8, Blocks.GRASS_BLOCK.stateId - 1) }

        assertEquals(false, feature.place(stone, random, DecorationPos(8, 40, 8)))
        assertTrue(feature.place(snowyGrass, random, DecorationPos(8, 40, 8)))
    }

    @Test
    fun paleMossCarpetBuildsTheExactTwoLayerWallState() {
        val paleMossCarpet = stateId("pale_moss_carpet")
        var result: Chunk? = null
        for (seed in 0L..100L) {
            val chunk = Chunk(0, 0)
            chunk.setBlockStateId(8, 39, 8, Blocks.STONE.stateId)
            chunk.setBlockStateId(8, 40, 7, Blocks.STONE.stateId)
            chunk.setBlockStateId(8, 41, 7, Blocks.STONE.stateId)
            SimpleBlockFeature({ _, _ -> paleMossCarpet })
                .place(chunk, WorldgenRandom(XoroshiroRandom(seed)), DecorationPos(8, 40, 8))
            if (Blocks.byStateId(chunk.getBlockStateId(8, 41, 8))?.block?.path == "pale_moss_carpet") {
                result = chunk
                break
            }
        }

        val chunk = assertNotNull(result, "expected one seed to retain the eligible topper side")
        val bottom = assertNotNull(Blocks.byStateId(chunk.getBlockStateId(8, 40, 8)))
        val topper = assertNotNull(Blocks.byStateId(chunk.getBlockStateId(8, 41, 8)))
        assertEquals("true", bottom.properties["bottom"])
        assertEquals("tall", bottom.properties["north"])
        assertEquals("false", topper.properties["bottom"])
        assertEquals("low", topper.properties["north"])
        assertTrue(listOf("east", "south", "west").all { bottom.properties[it] == "none" })
        assertTrue(listOf("east", "south", "west").all { topper.properties[it] == "none" })
    }

    @Test
    fun paleMossCarpetRejectsPartialAttachmentFaces() {
        val paleMossCarpet = stateId("pale_moss_carpet")
        val bottomSlab =
            Blocks.stateIdOf(
                Identifier.vanilla("stone_slab"),
                mapOf("type" to "bottom", "waterlogged" to "false"),
            ) ?: error("missing bottom stone slab")
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 39, 8, Blocks.STONE.stateId)
        chunk.setBlockStateId(8, 40, 7, bottomSlab)
        chunk.setBlockStateId(8, 41, 7, bottomSlab)

        assertTrue(
            SimpleBlockFeature({ _, _ -> paleMossCarpet })
                .place(chunk, WorldgenRandom(XoroshiroRandom(1L)), DecorationPos(8, 40, 8)),
        )

        val bottom = assertNotNull(Blocks.byStateId(chunk.getBlockStateId(8, 40, 8)))
        assertEquals("none", bottom.properties["north"])
        assertTrue(Blocks.isAir(chunk.getBlockStateId(8, 41, 8)))
    }

    @Test
    fun paleMossTopperUsesTheWorldgenRegionRandomWithoutAdvancingTheFeatureRandom() {
        val featureRandom = WorldgenRandom(XoroshiroRandom(91L))
        val expectedFeatureRandom = WorldgenRandom(XoroshiroRandom(91L))
        val regionRandom = XoroshiroRandom(7L)
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 39, 8, Blocks.STONE.stateId)
        chunk.setBlockStateId(8, 40, 7, Blocks.STONE.stateId)
        chunk.setBlockStateId(8, 41, 7, Blocks.STONE.stateId)

        SimpleBlockFeature({ _, _ -> stateId("pale_moss_carpet") })
            .place(chunk, featureRandom, DecorationPos(8, 40, 8), regionRandom)

        assertEquals(expectedFeatureRandom.nextLong(), featureRandom.nextLong())
    }

    @Test
    fun scheduledSimpleBlockKeepsItsTickAcrossTheChunkBorder() {
        val chunk = Chunk(0, 0)
        val flowerStateId = stateId("closed_eyeblossom")
        chunk.setDecorationBlockStateId(16, 39, 4, Blocks.GRASS_BLOCK.stateId)

        assertTrue(
            SimpleBlockFeature({ _, _ -> flowerStateId }, scheduleTick = true)
                .place(chunk, WorldgenRandom(XoroshiroRandom(1L)), DecorationPos(16, 40, 4)),
        )

        val tick = chunk.takeGeneratedBlockTicks().single()
        assertEquals(16, tick.x)
        assertEquals(40, tick.y)
        assertEquals(4, tick.z)
        assertEquals(flowerStateId, tick.expectedStateId)
        assertEquals("closed_eyeblossom", tick.type.path)
        assertEquals(1, tick.delay)
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing $path")
}
