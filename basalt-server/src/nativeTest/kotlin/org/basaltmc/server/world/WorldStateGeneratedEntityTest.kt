package org.basaltmc.server.world

import org.basaltmc.common.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkGeneratedEntity
import kotlin.test.Test
import kotlin.test.assertEquals

class WorldStateGeneratedEntityTest {
    @Test
    fun runtimeIdentityIsStableUntilItsChunkIsEvicted() {
        fun generatedChunk(): Chunk =
            Chunk(2, 3).apply {
                addGeneratedEntity(
                    ChunkGeneratedEntity(
                        Identifier.vanilla("end_crystal"),
                        40.5,
                        95.0,
                        56.5,
                        90.0f,
                        0.0f,
                    ),
                )
                markClean()
            }

        var nextId = 20
        val world = WorldState(listOf(generatedChunk())) { _, _ -> generatedChunk() }

        assertEquals(20, world.generatedEntities { nextId++ }.single().entityId)
        assertEquals(20, world.generatedEntities { nextId++ }.single().entityId)
        assertEquals(21, nextId, "reading the same loaded descriptor must not allocate twice")

        assertEquals(1, world.evictClean(world.evictionCandidates { _, _ -> false }))
        world.getOrGenerateChunk(2, 3)
        assertEquals(21, world.generatedEntities { nextId++ }.single().entityId)
    }
}
