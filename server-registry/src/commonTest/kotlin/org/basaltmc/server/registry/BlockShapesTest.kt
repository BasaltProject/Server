package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BlockShapesTest {
    private fun state(name: String): Int =
        requireNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla(name))) { "No block called $name" }

    @Test
    fun aFullBlockFillsItsCubeAndStopsLight() {
        val stone = state("stone")
        assertTrue(BlockShapes.isFullCube(stone))
        assertEquals(15, BlockShapes.dampening(stone))
        for (face in BlockFace.entries) assertTrue(BlockShapes.isFaceSturdy(stone, face))
    }

    @Test
    fun glassFillsItsCubeWithoutStoppingLight() {
        val glass = state("glass")
        assertTrue(BlockShapes.isFullCube(glass))
        assertEquals(0, BlockShapes.dampening(glass))
    }

    @Test
    fun aBottomSlabIsSturdyUnderfootOnly() {
        val slab = state("stone_slab")
        assertFalse(BlockShapes.isFullCube(slab))
        assertEquals(0, BlockShapes.dampening(slab))
        assertTrue(BlockShapes.isFaceSturdy(slab, BlockFace.DOWN))
        assertFalse(BlockShapes.isFaceSturdy(slab, BlockFace.UP))
    }

    @Test
    fun aTorchHasNoShapeAtAll() {
        val torch = state("torch")
        assertTrue(BlockShapes.isEmptyShape(torch))
        assertEquals(0, BlockShapes.dampening(torch))
    }

    @Test
    fun waterDimsLightWithoutBlockingIt() {
        assertEquals(1, BlockShapes.dampening(Blocks.WATER.stateId))
    }

    @Test
    fun aFullBlockAlwaysOccludes() {
        val stone = state("stone")
        val torch = state("torch")
        assertTrue(BlockShapes.mergedFaceOccludes(stone, torch, BlockFace.EAST))
        assertTrue(BlockShapes.mergedFaceOccludes(torch, stone, BlockFace.EAST))
    }

    @Test
    fun twoHalvesOfTheSameFaceOccludeTogetherAndNeitherAlone() {
        // The two ids the generated table gives `stone_slab[type=bottom]` and `[type=top]`, both
        // dry: a bottom slab covers the lower half of the face between them, a top slab the upper.
        val bottom = state("stone_slab")
        val top = bottom - 2
        assertFalse(BlockShapes.mergedFaceOccludes(bottom, bottom, BlockFace.EAST))
        assertFalse(BlockShapes.mergedFaceOccludes(top, top, BlockFace.EAST))
        assertTrue(BlockShapes.mergedFaceOccludes(bottom, top, BlockFace.EAST))
    }

    @Test
    fun aSlabWallsOffTheBlockUnderIt() {
        val slab = state("stone_slab")
        val torch = state("torch")
        assertTrue(BlockShapes.mergedFaceOccludes(slab, torch, BlockFace.DOWN))
        assertFalse(BlockShapes.mergedFaceOccludes(slab, torch, BlockFace.UP))
    }
}
