package org.basaltmc.server.world

import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the view-distance deltas. The lists are compared sorted: the
 * Kotlin emitted them in its insertion-ordered set's order, which is an artifact of that container.
 */
class ChunkTrackerOracle {
    private fun render(positions: List<ChunkPos>): String =
        positions.sortedWith(compareBy({ it.x }, { it.z })).joinToString(";") { "${it.x},${it.z}" }

    @Test
    fun dump() {
        val tracker = ChunkTracker()
        val steps = listOf(Triple(0, 0, 2), Triple(1, 0, 2), Triple(1, 0, 3), Triple(-4, 7, 1), Triple(-4, 7, 0))
        for ((index, step) in steps.withIndex()) {
            val (x, z, radius) = step
            val delta = tracker.update(x, z, radius)
            println(
                "ORACLE step$index load=${render(delta.toLoad)} unload=${render(delta.toUnload)} " +
                    "loaded=${tracker.loaded.size}",
            )
        }
        tracker.reset()
        val afterReset = tracker.update(-4, 7, 0)
        println(
            "ORACLE reset load=${render(afterReset.toLoad)} unload=${render(afterReset.toUnload)} " +
                "loaded=${tracker.loaded.size}",
        )
    }
}
