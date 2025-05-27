package org.basaltmc.server.world

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Unloading is the difference between bounded memory and memory that only grows, but it is also the
 * one place a mistake silently deletes what a player built. These cover both halves.
 */
class WorldStateEvictionTest {
    private fun worldWith(vararg positions: Pair<Int, Int>): WorldState =
        WorldState(positions.map { (x, z) -> Chunk(x, z) })

    @Test
    fun offersOnlyTheChunksThePredicateRejects() {
        val world = worldWith(0 to 0, 10 to 10)

        val candidates = world.evictionCandidates { x, z -> x == 0 && z == 0 }

        assertEquals(1, candidates.size)
        assertEquals(10 to 10, candidates.single().let { it.chunkX to it.chunkZ })
    }

    @Test
    fun evictingRemovesTheChunkFromMemory() {
        val world = worldWith(0 to 0, 10 to 10)

        val removed = world.evictClean(world.evictionCandidates { x, _ -> x == 0 })

        assertEquals(1, removed)
        assertEquals(1, world.loadedChunkCount)
        assertNotNull(world.chunkIfLoaded(0, 0))
        assertNull(world.chunkIfLoaded(10, 10))
    }

    @Test
    fun refusesToEvictAChunkWithUnsavedChanges() {
        val world = worldWith(10 to 10)
        val chunk = world.chunkIfLoaded(10, 10)
        assertNotNull(chunk)
        chunk.setBlockStateId(0, 70, 0, Blocks.STONE.stateId)

        val removed = world.evictClean(world.evictionCandidates { _, _ -> false })

        assertEquals(0, removed, "a dirty chunk must survive until its changes are on disk")
        assertEquals(1, world.loadedChunkCount)
    }

    @Test
    fun evictsOnceTheChangesHaveBeenSaved() {
        val world = worldWith(10 to 10)
        val chunk = world.chunkIfLoaded(10, 10)
        assertNotNull(chunk)
        chunk.setBlockStateId(0, 70, 0, Blocks.STONE.stateId)
        chunk.markClean()

        assertEquals(1, world.evictClean(world.evictionCandidates { _, _ -> false }))
        assertEquals(0, world.loadedChunkCount)
    }

    @Test
    fun keepingEverythingEvictsNothing() {
        val world = worldWith(0 to 0, 10 to 10)

        assertTrue(world.evictionCandidates { _, _ -> true }.isEmpty())
        assertEquals(2, world.loadedChunkCount)
    }

    @Test
    fun anEvictedChunkComesBackFromTheLoader() {
        var loads = 0
        val world =
            WorldState(listOf(Chunk(10, 10))) { x, z ->
                loads++
                Chunk(x, z)
            }
        world.evictClean(world.evictionCandidates { _, _ -> false })

        world.getOrGenerateChunk(10, 10)

        assertEquals(1, loads, "revisiting an unloaded chunk must go back through the loader")
        assertEquals(1, world.loadedChunkCount)
    }
}
