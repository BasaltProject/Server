package org.basaltmc.world

import org.basaltmc.common.Identifier
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.registry.Blocks
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Saving only what changed is only safe if "changed" is never under-reported: a chunk that says it
 * is clean when it is not loses whatever the player built in it.
 */
class ChunkDirtyTest {
    @Test
    fun `a fresh chunk has nothing worth saving`() {
        assertFalse(Chunk(0, 0).isDirty)
    }

    @Test
    fun `placing a block marks the chunk dirty`() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(1, 70, 1, Blocks.STONE.stateId)
        assertTrue(chunk.isDirty)
    }

    @Test
    fun `breaking a block marks the chunk dirty`() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(1, 70, 1, Blocks.STONE.stateId)
        chunk.markClean()

        chunk.setBlockStateId(1, 70, 1, Blocks.AIR.stateId)

        assertTrue(chunk.isDirty, "removing a block is as much a change as adding one")
    }

    @Test
    fun `writing the same block back does not mark the chunk dirty`() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(1, 70, 1, Blocks.STONE.stateId)
        chunk.markClean()

        chunk.setBlockStateId(1, 70, 1, Blocks.STONE.stateId)

        assertFalse(chunk.isDirty, "a no-op write must not cost a region rewrite")
    }

    @Test
    fun `a block entity marks the chunk dirty`() {
        val chunk = Chunk(0, 0)
        chunk.setBlockEntity(ChunkBlockEntity(1, 70, 1, Identifier.vanilla("chest"), NbtCompound()))
        assertTrue(chunk.isDirty)
    }

    @Test
    fun `a biome change marks the chunk dirty`() {
        val chunk = Chunk(0, 0)
        chunk.setBiomeId(0, 0, 0, 1)
        assertTrue(chunk.isDirty)
    }

    @Test
    fun `marking clean holds until the next real change`() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(1, 70, 1, Blocks.STONE.stateId)
        chunk.markClean()
        assertFalse(chunk.isDirty)

        chunk.setBlockStateId(2, 70, 2, Blocks.DIRT.stateId)
        assertTrue(chunk.isDirty)
    }

    @Test
    fun `a generated chunk asks to be saved`() {
        val generated = NoiseWorldGenerator.generate(0, 0)
        assertTrue(generated.isDirty, "a chunk that exists nowhere on disk yet must be saved")
    }
}
