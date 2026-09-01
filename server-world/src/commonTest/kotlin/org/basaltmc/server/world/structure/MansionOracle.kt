package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the woodland mansion's floor plan and its piece list. */
class MansionOracle {
    private fun describe(piece: MansionPiece): String =
        "${piece.template}@${piece.position.x},${piece.position.y},${piece.position.z}/" +
            "${piece.rotation.name}/${piece.mirror.name}"

    private fun foldGrid(
        grid: SimpleGrid,
        checksum: Long,
    ): Long {
        var value = checksum
        for (y in 0 until grid.height) {
            for (x in 0 until grid.width) value = (value xor grid[x, y].toLong()) * 1099511628211L
        }
        return value
    }

    private fun report(
        name: String,
        seed: Long,
        rotation: StructureRotation,
        mansions: Int,
    ) {
        var checksum = -3750763034362895579L
        var pieces = 0
        var templates = 0
        var first = "none"
        val names = mutableSetOf<String>()
        var draws = 0
        for (index in 0 until mansions) {
            val random = WorldgenRandom(LegacyRandom(seed + index))
            val grid = MansionGrid(random)
            checksum = foldGrid(grid.baseGrid, checksum)
            checksum = foldGrid(grid.thirdFloorGrid, checksum)
            for (floor in 0 until 3) checksum = foldGrid(grid.floorRooms[floor], checksum)
            val list = mutableListOf<MansionPiece>()
            MansionPlacer(random, MansionRoomFiller(random))
                .createMansion(StructurePos(index * 3 - 10, 64, index * -2 + 7), rotation, list, grid)
            if (index == 0) first = list.firstOrNull()?.let(::describe) ?: "none"
            pieces += list.size
            for (piece in list) {
                names += piece.template
                for (char in describe(piece)) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            }
            draws = draws * 31 + random.nextInt(1 shl 20)
        }
        templates = names.size
        println(
            "ORACLE $name pieces=$pieces templates=$templates first=$first draws=$draws checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("north", 90210L, StructureRotation.NONE, 16)
        // The whole house turns with its origin, so a rotation moves every piece and its own turn.
        report("clockwise", 90210L, StructureRotation.CLOCKWISE_90, 16)
        report("otherSeed", 13L, StructureRotation.NONE, 16)
    }
}
