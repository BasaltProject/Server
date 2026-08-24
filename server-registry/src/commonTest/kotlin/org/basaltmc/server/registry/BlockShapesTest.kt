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
        assertFalse(BlockShapes.isSolidRender(glass))
        assertEquals(0, BlockShapes.dampening(glass))
    }

    @Test
    fun occludingCubesRenderSolid() {
        assertTrue(BlockShapes.isSolidRender(state("stone")))
        assertTrue(BlockShapes.isSolidRender(state("dirt")))
        assertFalse(BlockShapes.isSolidRender(state("oak_leaves")))
        assertFalse(BlockShapes.isSolidRender(state("stone_slab")))
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
    fun slabFaceOcclusionDependsOnItsHalf() {
        val bottom = state("stone_slab")
        val top = bottom - 2

        assertTrue(BlockShapes.isFaceOcclusionFull(bottom, BlockFace.DOWN))
        assertFalse(BlockShapes.isFaceOcclusionFull(bottom, BlockFace.UP))
        assertFalse(BlockShapes.isFaceOcclusionFull(top, BlockFace.DOWN))
        assertTrue(BlockShapes.isFaceOcclusionFull(top, BlockFace.UP))
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

    /**
     * What gives way to a placed block and what does not, checked against the jar rather than
     * guessed: grass and a snow layer are replaced, a flower and a firefly bush are built on top
     * of, and so are the leaves of a tree. Leaf litter, which lies on the ground, is replaced.
     */
    @Test
    fun onlyWhatVanillaSaysIsReplaceable() {
        for (name in listOf(
            "air",
            "water",
            "lava",
            "short_grass",
            "tall_grass",
            "fern",
            "snow",
            "dead_bush",
            "vine",
            "fire",
            "leaf_litter",
            "seagrass",
        )) {
            assertTrue(BlockShapes.isReplaceable(state(name)), "$name should give way")
        }
        for (name in listOf(
            "dandelion",
            "poppy",
            "firefly_bush",
            "oak_leaves",
            "oak_sapling",
            "moss_carpet",
            "stone",
        )) {
            assertFalse(BlockShapes.isReplaceable(state(name)), "$name should not give way")
        }
    }

    /** parity: `BlockState.isSolid`, which is what lets a bucket be emptied into water. */
    @Test
    fun aFluidIsNotSolid() {
        assertFalse(BlockShapes.isSolidState(Blocks.WATER.stateId))
        assertFalse(BlockShapes.isSolidState(state("lava")))
        assertTrue(BlockShapes.isSolidState(Blocks.STONE.stateId))
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
