package org.basaltmc.server.world.region

import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtString
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The entities a chunk carries between one run and the next. */
class ChunkNbtEntitiesTest {
    private fun entity(id: String): NbtCompound = NbtCompound(mapOf("id" to NbtString(id)))

    @Test
    fun aChunkCarriesItsEntitiesToDiskAndBack() {
        val chunk = Chunk(2, -3)
        chunk.setSavedEntities(listOf(entity("minecraft:zombie"), entity("minecraft:cow")))

        val restored = ChunkNbt.read(ChunkNbt.write(chunk), chunk.verticalBounds)
        val entities = restored.savedEntities()
        assertEquals(2, entities.size)
        assertEquals("minecraft:zombie", entities.first().getString("id"))
    }

    @Test
    fun takingThemLeavesNoneBehind() {
        val chunk = Chunk(0, 0)
        chunk.setSavedEntities(listOf(entity("minecraft:cow")))
        assertEquals(1, chunk.takeSavedEntities().size)
        assertTrue(chunk.savedEntities().isEmpty())
    }

    @Test
    fun aChunkWithNoneWritesNoneAndStaysClean() {
        val chunk = Chunk(0, 0)
        chunk.markClean()
        chunk.setSavedEntities(emptyList())
        assertTrue(chunk.savedEntities().isEmpty())
        assertTrue(!chunk.isDirty)
    }
}
