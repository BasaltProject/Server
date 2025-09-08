package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * // parity: water over soul sand or magma schedules a tick running `BubbleColumnBlock.updateColumn`,
 * so a generated chunk is saved with its columns. Nothing ticks yet, so it is built after decoration.
 */
public object BubbleColumnFormation {
    public fun formInto(chunk: Chunk) {
        if (WATER == Blocks.AIR.stateId) return
        val minY = chunk.verticalBounds.minY
        val maxY = chunk.verticalBounds.maxY
        for (z in 0..15) {
            for (x in 0..15) {
                for (y in minY + 1..maxY) {
                    if (chunk.getBlockStateId(x, y, z) != WATER) continue
                    val columnStateId = columnStateFor(chunk.getBlockStateId(x, y - 1, z)) ?: continue
                    var top = y
                    while (top <= maxY && chunk.getBlockStateId(x, top, z) == WATER) {
                        chunk.setBlockStateId(x, top, z, columnStateId)
                        top++
                    }
                }
            }
        }
    }

    /** // parity: getColumnState. Soul sand pushes up, magma drags down, anything else grows nothing. */
    private fun columnStateFor(belowStateId: Int): Int? =
        when (Blocks.byStateId(belowStateId)?.block?.path) {
            "soul_sand" -> PUSH_UP
            "magma_block" -> DRAG_DOWN
            else -> null
        }

    private fun state(drag: Boolean): Int =
        Blocks.stateIdOf(Identifier.vanilla("bubble_column"), mapOf("drag" to drag.toString()))
            ?: Blocks.AIR.stateId

    private val WATER = Blocks.WATER.stateId
    private val PUSH_UP = state(drag = false)
    private val DRAG_DOWN = state(drag = true)
}
