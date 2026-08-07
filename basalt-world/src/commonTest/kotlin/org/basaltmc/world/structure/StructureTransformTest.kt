package org.basaltmc.world.structure

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`StructureTemplate.transform(BlockPos, Mirror, Rotation, BlockPos)`) through a small JVM
 * harness for every Mirror/Rotation combination, not by re-deriving the algorithm from memory.
 */
class StructureTransformTest {
    private val pivot = StructurePos(3, 0, 2)
    private val pos = StructurePos(5, 7, 1)

    private fun transform(
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): StructurePos = StructureTransform.transform(pos, mirror, rotation, pivot)

    @Test
    fun matchesVanillaForEveryMirrorRotationCombination() {
        assertEquals(StructurePos(5, 7, 1), transform(StructureMirror.NONE, StructureRotation.NONE))
        assertEquals(StructurePos(4, 7, 4), transform(StructureMirror.NONE, StructureRotation.CLOCKWISE_90))
        assertEquals(StructurePos(1, 7, 3), transform(StructureMirror.NONE, StructureRotation.CLOCKWISE_180))
        assertEquals(StructurePos(2, 7, 0), transform(StructureMirror.NONE, StructureRotation.COUNTERCLOCKWISE_90))

        assertEquals(StructurePos(5, 7, -1), transform(StructureMirror.LEFT_RIGHT, StructureRotation.NONE))
        assertEquals(StructurePos(6, 7, 4), transform(StructureMirror.LEFT_RIGHT, StructureRotation.CLOCKWISE_90))
        assertEquals(StructurePos(1, 7, 5), transform(StructureMirror.LEFT_RIGHT, StructureRotation.CLOCKWISE_180))
        assertEquals(
            StructurePos(0, 7, 0),
            transform(StructureMirror.LEFT_RIGHT, StructureRotation.COUNTERCLOCKWISE_90),
        )

        assertEquals(StructurePos(-5, 7, 1), transform(StructureMirror.FRONT_BACK, StructureRotation.NONE))
        assertEquals(StructurePos(4, 7, -6), transform(StructureMirror.FRONT_BACK, StructureRotation.CLOCKWISE_90))
        assertEquals(StructurePos(11, 7, 3), transform(StructureMirror.FRONT_BACK, StructureRotation.CLOCKWISE_180))
        assertEquals(
            StructurePos(2, 7, 10),
            transform(StructureMirror.FRONT_BACK, StructureRotation.COUNTERCLOCKWISE_90),
        )
    }
}
