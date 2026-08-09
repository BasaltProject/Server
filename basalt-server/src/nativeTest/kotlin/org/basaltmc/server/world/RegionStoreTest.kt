package org.basaltmc.server.world

import org.basaltmc.io.FileIO
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

class RegionStoreTest {
    private val worldName = "region_store_test_world"
    private val dimension = "overworld"

    @BeforeTest
    fun setUp() = cleanUp()

    @AfterTest
    fun tearDown() = cleanUp()

    private fun cleanUp() {
        val dir = "$worldName/dimensions/minecraft/$dimension/region"
        for (name in FileIO.list(dir)) FileIO.delete("$dir/$name")
        FileIO.delete(dir)
        FileIO.delete("$worldName/dimensions/minecraft/$dimension")
        FileIO.delete("$worldName/dimensions/minecraft")
        FileIO.delete("$worldName/dimensions")
        FileIO.delete(worldName)
    }

    private fun store() = RegionStore(worldName, dimension)

    private fun chunkWith(
        x: Int,
        z: Int,
        stateId: Int,
    ): Chunk = Chunk(x, z).also { it.setBlockStateId(1, 70, 1, stateId) }

    @Test
    fun roundTripsAChunkThroughDisk() {
        store().saveChunks(listOf(chunkWith(0, 0, Blocks.STONE.stateId)), timestamp = 1)

        val loaded = store().loadChunk(0, 0)

        assertNotNull(loaded)
        assertEquals(Blocks.STONE.stateId, loaded.getBlockStateId(1, 70, 1))
    }

    @Test
    fun reportsNothingForAChunkNeverSaved() {
        assertNull(store().loadChunk(5, 5))
    }

    @Test
    fun aLoadedChunkIsNotConsideredUnsaved() {
        store().saveChunks(listOf(chunkWith(0, 0, Blocks.STONE.stateId)), timestamp = 1)

        val loaded = store().loadChunk(0, 0)

        assertNotNull(loaded)
        assertEquals(false, loaded.isDirty, "a chunk straight off disk must not ask to be written back")
    }

    @Test
    fun savingMarksTheChunksClean() {
        val chunk = chunkWith(0, 0, Blocks.STONE.stateId)
        assertEquals(true, chunk.isDirty)

        store().saveChunks(listOf(chunk), timestamp = 1)

        assertEquals(false, chunk.isDirty)
    }

    @Test
    fun countsRegionsWrittenNotChunks() {
        // Two chunks in the same region are one write; the third lives in another region.
        val written =
            store().saveChunks(
                listOf(
                    chunkWith(0, 0, Blocks.STONE.stateId),
                    chunkWith(1, 1, Blocks.DIRT.stateId),
                    chunkWith(40, 40, Blocks.STONE.stateId),
                ),
                timestamp = 1,
            )

        assertEquals(2, written)
    }

    @Test
    fun keepsEarlierChunksWhenAnotherInTheSameRegionIsSaved() {
        val store = store()
        store.saveChunks(listOf(chunkWith(0, 0, Blocks.STONE.stateId)), timestamp = 1)
        store.saveChunks(listOf(chunkWith(1, 1, Blocks.DIRT.stateId)), timestamp = 2)

        val fresh = store()
        assertEquals(Blocks.STONE.stateId, fresh.loadChunk(0, 0)?.getBlockStateId(1, 70, 1))
        assertEquals(Blocks.DIRT.stateId, fresh.loadChunk(1, 1)?.getBlockStateId(1, 70, 1))
    }

    @Test
    fun writesOutRegionsEvictedFromTheCache() {
        val store = store()
        // More regions than the cache holds, so the earliest are evicted mid-run and must survive.
        for (i in 0..6) store.saveChunks(listOf(chunkWith(i * 32, 0, Blocks.STONE.stateId)), timestamp = 1)
        store.flush()

        val fresh = store()
        for (i in 0..6) {
            assertEquals(
                Blocks.STONE.stateId,
                fresh.loadChunk(i * 32, 0)?.getBlockStateId(1, 70, 1),
                "region $i was lost when it fell out of the cache",
            )
        }
    }
}
