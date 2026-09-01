package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the stronghold maze. */
class StrongholdLayoutOracle {
    private fun describe(piece: StrongholdPiece): String {
        val box = piece.box
        return "${piece.kind.name}/${piece.orientation.name}/${piece.entryDoor.name}/${piece.depth}/" +
            "${box.minX},${box.minY},${box.minZ},${box.maxX},${box.maxY},${box.maxZ}/" +
            "${piece.leftLow}${piece.leftHigh}${piece.rightLow}${piece.rightHigh}/" +
            "${piece.leftChild}${piece.rightChild}/${piece.roomType}/${piece.tall}/${piece.steps}"
    }

    private fun report(
        name: String,
        seed: Long,
        seaLevel: Int,
        minY: Int,
        strongholds: Int,
    ) {
        var checksum = -3750763034362895579L
        var pieces = 0
        var libraries = 0
        var fillers = 0
        var first = "none"
        for (index in 0 until strongholds) {
            val random = WorldgenRandom(LegacyRandom(0L))
            val layout = StrongholdLayout(seaLevel, minY)
            val chunkX = index * 5 - 12
            val chunkZ = index * -4 + 7
            val built =
                layout.build(
                    random,
                    { attempt -> random.setLargeFeatureSeed(seed + index + attempt, chunkX, chunkZ) },
                    chunkX,
                    chunkZ,
                )
            if (index == 0) first = built.firstOrNull()?.let(::describe) ?: "none"
            pieces += built.size
            for (piece in built) {
                if (piece.kind == StrongholdPieceKind.LIBRARY) libraries++
                if (piece.kind == StrongholdPieceKind.FILLER_CORRIDOR) fillers++
                for (char in describe(piece)) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            }
        }
        println(
            "ORACLE $name pieces=$pieces libraries=$libraries fillers=$fillers first=$first checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("normal", 31337L, 63, -64, 12)
        // A shallow world leaves the maze where it grew instead of sinking it.
        report("shallow", 31337L, 20, 0, 12)
        report("otherSeed", 424242L, 63, -64, 12)
    }
}
