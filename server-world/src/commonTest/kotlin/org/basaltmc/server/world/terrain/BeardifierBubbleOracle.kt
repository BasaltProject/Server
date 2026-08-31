package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the beardifier and the bubble columns. */
class BeardifierBubbleOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun box(
        minX: Int,
        minY: Int,
        minZ: Int,
        maxX: Int,
        maxY: Int,
        maxZ: Int,
    ) = StructureBoundingBox(minX, minY, minZ, maxX, maxY, maxZ)

    @Test
    fun dumpBeardifier() {
        for (adjustment in TerrainAdjustment.entries) {
            val beardifier =
                StructureBeardifier(
                    listOf(
                        StructureBeardifier.Rigid(
                            box(-4, 60, -4, 4, 70, 4),
                            groundLevelDelta = 2,
                            adjustment = adjustment,
                        ),
                        StructureBeardifier.Rigid(
                            box(10, 55, 10, 14, 58, 14),
                            groundLevelDelta = 0,
                            adjustment = adjustment,
                        ),
                    ),
                    listOf(StructureBeardifier.Junction(2, 64, -3), StructureBeardifier.Junction(12, 57, 12)),
                )
            var checksum = -3750763034362895579L
            var nonZero = 0
            for (index in 0 until 2048) {
                val x = (index * 7).mod(40) - 12
                val y = (index * 3).mod(30) + 50
                val z = (index * 11).mod(40) - 12
                val value = beardifier.compute(FunctionContext(x, y, z))
                if (value != 0.0) nonZero++
                checksum = (checksum xor value.toRawBits()) * 1099511628211L
            }
            println("ORACLE ${adjustment.name} nonZero=$nonZero checksum=$checksum")
        }
    }

    @Test
    fun dumpBubbleColumns() {
        val chunk = Chunk(2, -3)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val floor =
                    when ((x * 5 + z).mod(4)) {
                        0 -> stateId("soul_sand")
                        1 -> stateId("magma_block")
                        2 -> stateId("gravel")
                        else -> Blocks.STONE.stateId
                    }
                chunk.setBlockStateId(x, 40, z, floor)
                for (y in 41..48) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                // A lid over some columns, so the walk stops before the top of the water.
                if ((x + z).mod(3) == 0) chunk.setBlockStateId(x, 45, z, Blocks.STONE.stateId)
            }
        }
        BubbleColumnFormation.formInto(chunk)
        var checksum = -3750763034362895579L
        var columns = 0
        for (y in 39..50) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.byStateId(id)?.block?.path == "bubble_column") columns++
                }
            }
        }
        println("ORACLE bubbles columns=$columns checksum=$checksum")
    }
}
