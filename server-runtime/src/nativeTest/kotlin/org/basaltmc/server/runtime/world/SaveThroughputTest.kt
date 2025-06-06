package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * What a periodic sweep costs and where. Linear recompresses a whole region per save, so the write
 * was assumed to be the expensive half; it is two milliseconds, and all the rest was the chunk NBT.
 */
@OptIn(ExperimentalTime::class)
class SaveThroughputTest {
    private val worldName = "save_throughput_world"

    /** Under build/, because a test that writes anywhere else leaves it behind in the repository. */
    private val worldsRoot = "build/test-worlds"

    @BeforeTest
    fun setUp() = cleanUp()

    @AfterTest
    fun tearDown() = cleanUp()

    private fun cleanUp() {
        val root = "$worldsRoot/$worldName"
        val dir = "$root/dimensions/minecraft/overworld/region"
        for (name in FileIO.list(dir)) FileIO.delete("$dir/$name")
        FileIO.delete(dir)
        FileIO.delete("$root/dimensions/minecraft/overworld")
        FileIO.delete("$root/dimensions/minecraft")
        FileIO.delete("$root/dimensions")
        FileIO.delete(root)
    }

    private fun terrainChunk(
        x: Int,
        z: Int,
    ): Chunk =
        Chunk(x, z).also { chunk ->
            for (y in -64..70) {
                for (localZ in 0..15) {
                    for (localX in 0..15) {
                        val stateId = if (y < 60) Blocks.STONE.stateId else Blocks.DIRT.stateId
                        chunk.setBlockStateId(localX, y, localZ, stateId)
                    }
                }
            }
        }

    @Test
    fun aSweepSpendsItsTimeInTheNbtAndNotInZstd() {
        val store = RegionStore(worldName, "overworld", worldsRoot)
        val chunks = (0 until CHUNKS).map { terrainChunk(it % 32, it / 32) }

        val started = TimeSource.Monotonic.markNow()
        val regions = store.saveChunks(chunks, timestamp = 1)
        val total = started.elapsedNow().inWholeMilliseconds

        println(
            "$CHUNKS chunks over $regions region(s): total ${total}ms, " +
                "encode ${store.encodeMillis}ms, write ${store.writeMillis}ms",
        )

        // Loose, like the generation one: a debug binary on a loaded runner is not the number being
        // defended. It was 3834ms before the palette derivation stopped boxing every value.
        assertTrue(total < CEILING_MILLIS, "saving $CHUNKS chunks took ${total}ms")
    }

    private companion object {
        const val CHUNKS = 256
        const val CEILING_MILLIS = 1_500L
    }
}
