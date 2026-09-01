package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the monument's lattice and its room fitting. */
class MonumentRoomsOracle {
    private fun describe(room: MonumentRoom): String =
        "${room.index}/${room.claimed}/${room.isSource}/${room.countOpenings()}/" +
            (0 until 6).joinToString("") { if (room.hasOpening[it]) "1" else "0" }

    private fun report(
        name: String,
        seed: Long,
        monuments: Int,
    ) {
        var checksum = -3750763034362895579L
        var rooms = 0
        var openings = 0
        var pieces = 0
        val kinds = IntArray(MonumentRoomKind.entries.size)
        var draws = 0
        for (index in 0 until monuments) {
            val random = WorldgenRandom(LegacyRandom(seed + index))
            val graph = generateMonumentRoomGraph(random)
            rooms += graph.rooms.size
            for (room in graph.rooms) {
                openings += room.countOpenings()
                for (char in describe(room)) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            }
            checksum = (checksum xor graph.coreRoom.index.toLong()) * 1099511628211L
            val fitted = fitMonumentRooms(graph, random)
            pieces += fitted.size
            for (piece in fitted) {
                kinds[piece.kind.ordinal]++
                checksum = (checksum xor (piece.room?.index ?: -1).toLong()) * 1099511628211L
                checksum = (checksum xor piece.design.toLong()) * 1099511628211L
                checksum = (checksum xor piece.kind.ordinal.toLong()) * 1099511628211L
            }
            draws = draws * 31 + random.nextInt(1 shl 20)
        }
        println(
            "ORACLE $name rooms=$rooms openings=$openings pieces=$pieces draws=$draws " +
                "kinds=${kinds.joinToString(",")} checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("first", 20250831L, 24)
        report("second", 1L, 24)
        report("third", -777L, 24)
    }
}
