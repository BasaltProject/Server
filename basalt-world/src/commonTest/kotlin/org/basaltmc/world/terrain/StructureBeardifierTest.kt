package org.basaltmc.world.terrain

import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.structure.StructureBoundingBox
import kotlin.test.Test
import kotlin.test.assertEquals

class StructureBeardifierTest {
    @Test
    fun beardThinRigidMatchesOfficial26_2Vectors() {
        val box = StructureBoundingBox(10, 40, 20, 14, 44, 24)
        val beardifier = StructureBeardifier(listOf(StructureBeardifier.Rigid(box)))
        val vectors =
            listOf(
                Vector(12, 40, 22, -0.55677576530078),
                Vector(12, 39, 22, 0.55677576530078),
                Vector(12, 41, 22, -0.49142526044466295),
                Vector(9, 40, 22, -0.23356892149243513),
                Vector(0, 40, 22, -5.36643042613213E-5),
                Vector(14, 51, 24, -1.4534922367297665E-4),
                Vector(14, 52, 24, 0.0),
            )

        for (vector in vectors) {
            assertEquals(
                vector.density,
                beardifier.compute(FunctionContext(vector.x, vector.y, vector.z)),
                1.0E-16,
                vector.toString(),
            )
        }
    }

    private data class Vector(
        val x: Int,
        val y: Int,
        val z: Int,
        val density: Double,
    )
}
