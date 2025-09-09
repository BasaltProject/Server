package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A tree records its logs and leaves by observing its own writes. // parity: `TreeFeature` feeds its
 * consumer from the placement call, not from whether the level changed, so a leaf written twice stays
 * in the context and a vine may still hang off it. Skipping the no-op silently shrinks every tree.
 */
class BlockWriteObserverTest {
    private fun state(path: String) =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    @Test
    fun `a write that changes nothing is still observed`() {
        val chunk = Chunk(0, 0)
        val leaves = state("oak_leaves")
        chunk.setBlockStateId(3, 70, 4, leaves)

        val seen = mutableListOf<Triple<Int, Int, Int>>()
        val changed =
            chunk.observeBlockWrites({ x, y, z, _, _ -> seen += Triple(x, y, z) }) {
                chunk.setBlockStateId(3, 70, 4, leaves)
            }
        assertTrue(!changed, "the block was already that state")
        assertEquals(listOf(Triple(3, 70, 4)), seen, "but the write must still reach the observer")
    }

    @Test
    fun `a deferred write that changes nothing is observed too`() {
        val chunk = Chunk(0, 0)
        val leaves = state("oak_leaves")
        val seen = mutableListOf<Triple<Int, Int, Int>>()
        chunk.withExternalDecorationBlockState(reader = { _, _, _ -> leaves }) {
            chunk.observeBlockWrites({ x, y, z, _, _ -> seen += Triple(x, y, z) }) {
                // One chunk to the east, where the neighbour already holds the same leaf.
                chunk.setDecorationBlockStateId(20, 70, 4, leaves)
            }
        }
        assertEquals(1, seen.size, "a no-op across the border still belongs to the decorator context")
    }
}
