package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.DecorationPos
import org.basaltmc.server.world.terrain.decoration.StandaloneFeatures
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A `LakeFeature` body is 16 wide, so only an origin at local (8, 8) fits inside one chunk. These lock
 * in that the body a chunk produces does not depend on where inside the chunk its origin sits.
 */
class LakeAcrossChunksTest {
    private val stone =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("stone"),
            )!!
            .stateId

    /** Every block of a solid world, so the only thing that can vary is where the origin sits. */
    private fun solidChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in LAKE_TEST_MIN_Y..LAKE_TEST_MAX_Y) setBlockStateId(x, y, z, stone)
                }
            }
        }

    private class Lake(
        val body: Map<Triple<Int, Int, Int>, Int>,
        val external: Int,
    )

    /** The lake written from [origin], as offsets from it, joining local and deferred writes. */
    private fun lakeAround(origin: DecorationPos): Lake {
        val chunk = solidChunk(origin.x shr 4, origin.z shr 4)
        val before = HashMap<Triple<Int, Int, Int>, Int>()
        val written = HashMap<Triple<Int, Int, Int>, Int>()
        for (dx in -9..9) {
            for (dz in -9..9) {
                for (dy in -6..6) {
                    before[Triple(dx, dy, dz)] = stone
                }
            }
        }
        val placed =
            chunk.withExternalDecorationBlockState(reader = { _, y, _ ->
                if (y in
                    LAKE_TEST_MIN_Y..LAKE_TEST_MAX_Y
                ) {
                    stone
                } else {
                    null
                }
            }) {
                StandaloneFeatures.placeAt(
                    chunk,
                    WorldgenRandom(XoroshiroRandom(LAKE_TEST_SEED)),
                    origin,
                    "lake_lava_surface",
                )
            }
        assertTrue(placed, "the preflight must accept a lake in solid ground at $origin")

        for ((offset, _) in before) {
            val (dx, dy, dz) = offset
            val x = origin.x + dx
            val y = origin.y + dy
            val z = origin.z + dz
            if (x shr 4 == chunk.chunkX && z shr 4 == chunk.chunkZ) {
                val stateId = chunk.getBlockStateId(x and 15, y, z and 15)
                if (stateId != stone) written[offset] = stateId
            }
        }
        var external = 0
        for (write in chunk.takeGeneratedBlockWrites()) {
            val offset = Triple(write.x - origin.x, write.y - origin.y, write.z - origin.z)
            if (offset in before && write.stateId != stone) {
                written[offset] = write.stateId
                external++
            }
        }
        return Lake(written, external)
    }

    @Test
    fun `a lake keeps its whole body wherever its origin sits in the chunk`() {
        val centred = lakeAround(DecorationPos(8, LAKE_TEST_ORIGIN_Y, 8))
        assertTrue(centred.body.size > 100, "a lake in solid ground carves a real volume")
        assertEquals(0, centred.external, "an origin at (8, 8) is the one that fits inside the chunk")

        // The four corners of the chunk, each of which pushes the 16-wide volume over two edges.
        for (
        origin in
        listOf(
            DecorationPos(0, LAKE_TEST_ORIGIN_Y, 0),
            DecorationPos(15, LAKE_TEST_ORIGIN_Y, 0),
            DecorationPos(0, LAKE_TEST_ORIGIN_Y, 15),
            DecorationPos(15, LAKE_TEST_ORIGIN_Y, 15),
        )
        ) {
            val corner = lakeAround(origin)
            assertEquals(centred.body, corner.body, "the body must not be clipped at origin $origin")
            assertTrue(corner.external > 0, "an origin at $origin must write into its neighbours")
        }
    }

    @Test
    fun `a lake body never wraps back into its own chunk`() {
        val corner = DecorationPos(0, LAKE_TEST_ORIGIN_Y, 0)
        val chunk = solidChunk(0, 0)
        chunk.withExternalDecorationBlockState(reader = { _, y, _ ->
            if (y in
                LAKE_TEST_MIN_Y..LAKE_TEST_MAX_Y
            ) {
                stone
            } else {
                null
            }
        }) {
            StandaloneFeatures.placeAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(LAKE_TEST_SEED)),
                corner,
                "lake_lava_surface",
            )
        }
        // Wrapping would show up as writes on the far side of the chunk, eight blocks from any
        // part of a body centred on (0, 0).
        for (x in 9 until 16) {
            for (z in 9 until 16) {
                for (y in LAKE_TEST_ORIGIN_Y - 5..LAKE_TEST_ORIGIN_Y + 4) {
                    assertEquals(stone, chunk.getBlockStateId(x, y, z), "wrapped write at $x/$y/$z")
                }
            }
        }
    }

    private companion object {
        const val LAKE_TEST_SEED = 4242L
        const val LAKE_TEST_ORIGIN_Y = 64
        const val LAKE_TEST_MIN_Y = 40
        const val LAKE_TEST_MAX_Y = 90
    }
}
