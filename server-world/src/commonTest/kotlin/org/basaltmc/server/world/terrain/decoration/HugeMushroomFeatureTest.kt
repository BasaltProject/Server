package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class HugeMushroomFeatureTest {
    @Test
    fun brownHugeMushroomPlacesStemAndCap() {
        val chunk = chunkWithMycelium(0)
        val random = WorldgenRandom(XoroshiroRandom(5L))
        val placed = HugeMushroomFeature.place(chunk, random, 8, 1, 8, HugeMushroomFeature.Kind.BROWN)
        assertTrue(placed, "expected the huge brown mushroom to place successfully")

        val stems = mutableListOf<Int>()
        val caps = mutableListOf<Int>()
        for (x in 0 until 16) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    when (Blocks.byStateId(id)?.block) {
                        stemBlock -> stems += id
                        brownCapBlock -> caps += id
                    }
                }
            }
        }
        // Minimum height 4, and the brown cap is a full 7x7-minus-corners layer.
        assertTrue(stems.size >= 4, "expected at least 4 mushroom stem blocks, got ${stems.size}")
        assertTrue(caps.size >= 32, "expected the brown cap layer (7x7 minus corners), got ${caps.size}")
        assertTrue(stems.all(::isExactStemState))
        assertTrue(
            caps.all { stateId ->
                val properties = Blocks.byStateId(stateId)?.properties ?: return@all false
                properties["down"] == "false" && properties["up"] == "true"
            },
        )
        assertTrue(
            caps.any { stateId ->
                val properties = Blocks.byStateId(stateId)?.properties ?: return@any false
                listOf("east", "north", "south", "west").all { properties[it] == "false" }
            },
        )
    }

    @Test
    fun redHugeMushroomPlacesStemAndCap() {
        val chunk = chunkWithMycelium(0)
        val random = WorldgenRandom(XoroshiroRandom(6L))
        val placed = HugeMushroomFeature.place(chunk, random, 8, 1, 8, HugeMushroomFeature.Kind.RED)
        assertTrue(placed, "expected the huge red mushroom to place successfully")

        val stems = mutableListOf<Int>()
        val caps = mutableListOf<Int>()
        for (x in 0 until 16) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    when (Blocks.byStateId(id)?.block) {
                        stemBlock -> stems += id
                        redCapBlock -> caps += id
                    }
                }
            }
        }
        assertTrue(stems.size >= 4, "expected at least 4 mushroom stem blocks, got ${stems.size}")
        assertTrue(caps.isNotEmpty(), "expected at least one red mushroom block")
        assertTrue(stems.all(::isExactStemState))
        assertTrue(caps.all { Blocks.byStateId(it)?.properties?.get("down") == "false" })
        assertEquals(setOf("true", "false"), caps.mapNotNull { Blocks.byStateId(it)?.properties?.get("up") }.toSet())
    }

    @Test
    fun derivedStatesStillMatchVanillaPlacementTagsByBlock() {
        val chunk = chunkWithMycelium(0)
        chunk.setBlockStateId(8, 0, 8, myceliumState - 1)
        chunk.setBlockStateId(8, 1, 8, Blocks.OAK_LEAVES.stateId - 16)

        val placed =
            HugeMushroomFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(6L)),
                8,
                1,
                8,
                HugeMushroomFeature.Kind.RED,
            )

        assertTrue(placed)
        assertEquals(stemBlock, Blocks.byStateId(chunk.getBlockStateId(8, 1, 8))?.block)
    }

    @Test
    fun capCrossingChunkEdgeKeepsWorldCoordinates() {
        val chunk = chunkWithMycelium(0)
        assertTrue(
            HugeMushroomFeature.place(
                chunk,
                WorldgenRandom(XoroshiroRandom(5L)),
                15,
                1,
                8,
                HugeMushroomFeature.Kind.BROWN,
            ),
        )

        val externalWrites = chunk.takeGeneratedBlockWrites()
        assertTrue(externalWrites.any { it.x >= 16 }, "expected the cap to continue into the east chunk")
        for (x in 0..2) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val block = Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block
                    assertTrue(block != brownCapBlock, "cap wrapped to local ($x,$y,$z)")
                }
            }
        }
    }

    private fun chunkWithMycelium(groundY: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, groundY, z, myceliumState)
            }
        }
        return chunk
    }

    private fun isExactStemState(stateId: Int): Boolean {
        val properties = Blocks.byStateId(stateId)?.properties ?: return false
        return properties ==
            linkedMapOf(
                "down" to "false",
                "east" to "true",
                "north" to "true",
                "south" to "true",
                "up" to "false",
                "west" to "true",
            )
    }

    private companion object {
        fun state(name: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

        val myceliumState = state("mycelium")
        val stemBlock = Identifier.vanilla("mushroom_stem")
        val brownCapBlock = Identifier.vanilla("brown_mushroom_block")
        val redCapBlock = Identifier.vanilla("red_mushroom_block")
    }
}
