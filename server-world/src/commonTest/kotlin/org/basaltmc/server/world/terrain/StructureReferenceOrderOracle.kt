package org.basaltmc.server.world.terrain

import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the order structure references come back in. It lives on its
 * own because no single generator ever holds enough of them at once to pin the order down.
 */
class StructureReferenceOrderOracle {
    private fun report(
        name: String,
        references: List<Pair<Int, Int>>,
    ) {
        val ordered = vanillaStructureReferenceChunkOrder(references)
        val text = ordered.joinToString(";") { "${it.first},${it.second}" }
        println("ORACLE $name size=${ordered.size} order=$text")
    }

    @Test
    fun dump() {
        report("empty", emptyList())
        report("one", listOf(3 to 5))
        // The origin is the one key the map keeps outside its table, and it comes back first.
        report("withOrigin", listOf(3 to 5, 0 to 0, -7 to 2))
        report("nine", (-1..1).flatMap { x -> (-1..1).map { z -> x to z } })
        report("row", (0 until 12).map { it to 0 })
        report("negatives", (0 until 9).map { -it to it - 4 })
        report("far", listOf(1000 to -1000, -1000 to 1000, 1000 to 1000, -1000 to -1000))
        report("full", (0 until 23).map { it * 7 to it * -3 })
    }
}
