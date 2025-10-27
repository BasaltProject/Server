package org.basaltmc.server.world.structure

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the mineshaft layout. */
class MineshaftLayoutOracle {
    private fun describe(piece: MineshaftPiece): String {
        val box = piece.box
        return "${piece.kind.name}/${piece.orientation?.name ?: "-"}/${piece.fillOrientation?.name ?: "-"}/" +
            "${box.minX},${box.minY},${box.minZ},${box.maxX},${box.maxY},${box.maxZ}/" +
            "${piece.genDepth}/${piece.hasRails}/${piece.spiderCorridor}/${piece.sections}/${piece.twoFloored}/" +
            piece.entrances.joinToString(";") {
                "${it.minX},${it.minY},${it.minZ},${it.maxX},${it.maxY},${it.maxZ}"
            }
    }

    private fun report(
        name: String,
        seed: Long,
        type: MineshaftType,
        seaLevel: Int,
        minY: Int,
        shafts: Int,
    ) {
        var checksum = -3750763034362895579L
        var pieces = 0
        var corridors = 0
        var crossings = 0
        var stairs = 0
        var entrances = 0
        var first = "none"
        for (index in 0 until shafts) {
            val assembled =
                MineshaftLayout.assemble(seed + index, index * 7 - 20, index * -3 + 11, type, seaLevel, minY)
            if (index == 0) first = assembled.firstOrNull()?.let(::describe) ?: "none"
            pieces += assembled.size
            for (piece in assembled) {
                when (piece.kind) {
                    MineshaftPiece.Kind.CORRIDOR -> corridors++
                    MineshaftPiece.Kind.CROSSING -> crossings++
                    MineshaftPiece.Kind.STAIRS -> stairs++
                    MineshaftPiece.Kind.ROOM -> Unit
                }
                entrances += piece.entrances.size
                for (char in describe(piece)) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            }
        }
        println(
            "ORACLE $name pieces=$pieces corridors=$corridors crossings=$crossings stairs=$stairs " +
                "entrances=$entrances first=$first checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("normal", 8877L, MineshaftType.NORMAL, 63, -64, 24)
        // A mesa shaft keeps the magic start height instead of sinking below sea level.
        report("mesa", 8877L, MineshaftType.MESA, 63, -64, 24)
        // A shallower world moves the whole shaft, and a low sea level can leave it where it was.
        report("shallow", 8877L, MineshaftType.NORMAL, 20, 0, 24)
        report("otherSeed", 5150L, MineshaftType.NORMAL, 63, -64, 24)
    }
}
