package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

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

        fun around(): List<WorldState.GeneratedWorldEntity> =
            world.generatedEntities({ nextId++ }, centerChunkX = 2, centerChunkZ = 3, radiusChunks = 1)

        assertEquals(20, around().single().entityId)
        assertEquals(20, around().single().entityId)
        assertEquals(21, nextId, "reading the same loaded descriptor must not allocate twice")

        assertEquals(1, world.evictClean(world.evictionCandidates { _, _ -> false }))
        world.getOrGenerateChunk(2, 3)
        assertEquals(21, around().single().entityId)
    }

    /** The scan is bounded, so a chunk outside the radius contributes nothing and costs nothing. */
    @Test
    fun onlyChunksWithinTheRadiusAreWalked() {
        val world = WorldState(listOf(chunkWithCrystal(2, 3), chunkWithCrystal(40, 40)))
        var nextId = 0

        val near = world.generatedEntities({ nextId++ }, centerChunkX = 2, centerChunkZ = 3, radiusChunks = 2)

        assertEquals(1, near.size, "a chunk 38 chunks away should not have been walked")
        assertEquals(1, nextId, "an entity outside the radius must not be given an id yet")
    }

    @Test
    fun installedStructureMobsAreHandedToTheRuntimeExactlyOnce() {
        val chunk =
            Chunk(2, 3).apply {
                addGeneratedEntity(
                    ChunkGeneratedEntity(
                        Identifier.vanilla("villager"),
                        40.5,
                        64.0,
                        56.5,
                        0f,
                        0f,
                        NbtCompound().apply { putString("id", "minecraft:villager") },
                    ),
                )
                addGeneratedEntity(
                    ChunkGeneratedEntity(Identifier.vanilla("item_frame"), 41.0, 65.0, 57.0, 0f, 0f),
                )
                markClean()
            }
        val world = WorldState(emptyList())

        world.install(chunk)

        assertEquals(listOf(Identifier.vanilla("villager")), world.takePendingGeneratedEntities().map { it.type })
        assertTrue(world.takePendingGeneratedEntities().isEmpty(), "a second tick must not materialize a duplicate")
        assertEquals(
            listOf(Identifier.vanilla("item_frame")),
            world.generatedEntities({ 1 }, 2, 3, 0).map { it.descriptor.type },
            "non-mob structure furniture stays on the immutable presentation path",
        )
        assertTrue(chunk.isDirty, "the removed descriptor must be persisted before a chunk reload")
    }

    private fun chunkWithCrystal(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ).apply {
            addGeneratedEntity(
                ChunkGeneratedEntity(Identifier.vanilla("end_crystal"), 0.5, 95.0, 0.5, 0f, 0f),
            )
            markClean()
        }
}
