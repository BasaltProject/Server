package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BlockSupportTest {
    private val air = Blocks.AIR.stateId
    private val stone = state("stone")

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties)) { "No state for $name" }

    /** The six neighbours, all air unless named. */
    private fun survives(
        stateId: Int,
        down: Int = air,
        up: Int = air,
        north: Int = air,
        south: Int = air,
        west: Int = air,
        east: Int = air,
        wet: Boolean = false,
    ) = BlockSupport.canSurvive(stateId, down, up, north, south, west, east, wet)

    @Test
    fun anOrdinaryBlockStandsAnywhere() {
        assertFalse(BlockSupport.needsSupport(stone))
        assertTrue(survives(stone))
    }

    @Test
    fun aTorchNeedsSomethingUnderIt() {
        val torch = state("torch")
        assertTrue(BlockSupport.needsSupport(torch))
        assertTrue(survives(torch, down = stone))
        assertFalse(survives(torch))
    }

    /**
     * A torch asks for the middle of the face, not the whole of it, which is why it sits on a fence
     * post and not on a bottom slab. Both answers come from the jar, not from what looks right.
     */
    @Test
    fun aTorchSitsOnAFencePostButNotOnABottomSlab() {
        val torch = state("torch")
        assertTrue(survives(torch, down = state("oak_fence")))
        assertFalse(survives(torch, down = state("stone_slab", mapOf("type" to "bottom"))))
        assertTrue(survives(torch, down = state("stone_slab", mapOf("type" to "top"))))
    }

    /** parity: `WallTorchBlock`, whose `facing` points away from the wall it hangs on. */
    @Test
    fun aWallTorchNeedsTheWallItFacesAwayFrom() {
        val torch = state("wall_torch", mapOf("facing" to "north"))
        assertTrue(survives(torch, south = stone))
        assertFalse(survives(torch, north = stone))
    }

    @Test
    fun aFlowerNeedsSoilAndNotJustAnySolidBlock() {
        val dandelion = state("dandelion")
        assertTrue(survives(dandelion, down = state("grass_block")))
        assertTrue(survives(dandelion, down = state("dirt")))
        assertTrue(survives(dandelion, down = state("farmland")))
        assertFalse(survives(dandelion, down = stone))
    }

    /** Sand alone is not enough: vanilla wants water beside the block underneath. */
    @Test
    fun sugarCaneOnlyStandsOnSandWhenThereIsWaterAbout() {
        val cane = state("sugar_cane")
        assertFalse(survives(cane, down = state("sand")))
        assertTrue(survives(cane, down = state("sand"), wet = true))
    }

    /**
     * The blocks whose rule one neighbour at a time cannot see. They are never broken, which is the
     * safe direction to be wrong in: a cactus that should have popped stays put, rather than a
     * block disappearing under someone.
     */
    @Test
    fun theBlocksThisBuildCannotJudgeAreLeftAlone() {
        for (name in listOf("cactus", "chorus_flower", "glow_lichen", "sculk_vein", "resin_clump")) {
            val stateId = state(name)
            assertFalse(BlockSupport.isModelled(stateId), "$name should not claim to be modelled")
            assertTrue(survives(stateId), "$name should never be broken by this")
        }
    }

    /** parity: `PistonHeadBlock.canSurvive`, which asks for the moving piston behind it, not the piston. */
    @Test
    fun aPistonHeadHangsOffTheMovingPistonBehindIt() {
        val head = state("piston_head", mapOf("facing" to "north"))
        assertTrue(survives(head, south = state("moving_piston")))
        assertFalse(survives(head, south = stone))
    }

    @Test
    fun cropsTickAtRandomAndStoneDoesNot() {
        assertTrue(BlockSupport.ticksRandomly(state("wheat", mapOf("age" to "3"))))
        assertTrue(BlockSupport.ticksRandomly(state("ice")))
        assertFalse(BlockSupport.ticksRandomly(stone))
        assertFalse(BlockSupport.ticksRandomly(air))
    }

    /** A door's upper half is held up by its own lower half, not by whatever is under that. */
    @Test
    fun theTopOfADoorRestsOnTheBottomOfIt() {
        val upper = state("oak_door", mapOf("half" to "upper", "facing" to "north"))
        val lower = state("oak_door", mapOf("half" to "lower", "facing" to "north"))
        assertTrue(survives(upper, down = lower))
        assertFalse(survives(upper, down = stone))
        assertTrue(survives(lower, down = stone))
    }
}
