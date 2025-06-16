package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Direction
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BlockPlacementTest {
    private fun placed(
        name: String,
        face: Direction = Direction.UP,
        clickY: Float = 0f,
        yaw: Float = 0f,
        pitch: Float = 0f,
        replacing: Int = Blocks.AIR.stateId,
    ): Map<String, String> {
        val block = Identifier.vanilla(name)
        val default = checkNotNull(Blocks.REGISTRY.idOf(block)) { "no block called $name" }
        val stateId = BlockPlacement.stateFor(block, default, face, clickY, yaw, pitch, replacing)
        return BlockStateDefinitions.propertiesOf(stateId)
    }

    /** parity: `RotatedPillarBlock`, whose axis is the axis of the face that was clicked. */
    @Test
    fun aLogLiesAlongTheFaceItWasPutOn() {
        assertEquals("y", placed("oak_log", face = Direction.UP)["axis"])
        assertEquals("x", placed("oak_log", face = Direction.EAST)["axis"])
        assertEquals("z", placed("oak_log", face = Direction.NORTH)["axis"])
    }

    /** parity: `AbstractFurnaceBlock`, facing back at whoever placed it. */
    @Test
    fun aFurnaceLooksBackAtThePlayer() {
        // Yaw 0 is looking south, so the front of the furnace looks north, back at the player.
        assertEquals("north", placed("furnace", yaw = 0f)["facing"])
        assertEquals("west", placed("furnace", yaw = -90f)["facing"])
        assertEquals("south", placed("furnace", yaw = 180f)["facing"])
    }

    /** parity: `StairBlock`, which faces the same way as the player rather than back at them. */
    @Test
    fun aStaircaseFacesTheSameWayAsThePlayer() {
        val downhill = placed("oak_stairs", yaw = 0f)
        assertEquals("south", downhill["facing"])
        assertEquals("bottom", downhill["half"])
        assertEquals("top", placed("oak_stairs", face = Direction.DOWN)["half"])
        assertEquals("top", placed("oak_stairs", face = Direction.NORTH, clickY = 0.8f)["half"])
    }

    @Test
    fun aSlabGoesWhereTheCursorWas() {
        assertEquals("bottom", placed("stone_slab", face = Direction.UP)["type"])
        assertEquals("top", placed("stone_slab", face = Direction.DOWN)["type"])
        assertEquals("bottom", placed("stone_slab", face = Direction.WEST, clickY = 0.2f)["type"])
        assertEquals("top", placed("stone_slab", face = Direction.WEST, clickY = 0.9f)["type"])
    }

    /** parity: `SlabBlock`, where a second slab in the same spot makes a double. */
    @Test
    fun aSlabOnASlabIsADoubleSlab() {
        val bottom = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab")))
        assertEquals("double", placed("stone_slab", replacing = bottom)["type"])
    }

    /** parity: `DispenserBlock`, pointing away from wherever the player is looking. */
    @Test
    fun aDispenserPointsAwayFromTheEyes() {
        assertEquals("north", placed("dispenser", yaw = 0f, pitch = 0f)["facing"])
        assertEquals("up", placed("dispenser", pitch = 90f)["facing"])
        assertEquals("down", placed("dispenser", pitch = -90f)["facing"])
    }

    /** parity: `LightningRodBlock`, which points out of the face it was stuck to. */
    @Test
    fun aLightningRodPointsOutOfTheFaceItWasClicked() {
        assertEquals("up", placed("lightning_rod", face = Direction.UP)["facing"])
        assertEquals("west", placed("lightning_rod", face = Direction.WEST)["facing"])
    }

    /** parity: `FaceAttachedHorizontalDirectionalBlock`: what it hangs on is the face that was clicked. */
    @Test
    fun aLeverHangsOnWhateverWasClicked() {
        val onFloor = placed("lever", face = Direction.UP, yaw = 0f)
        assertEquals("floor", onFloor["face"])
        assertEquals("south", onFloor["facing"], "on the floor it looks the way the player does")

        val onWall = placed("lever", face = Direction.WEST)
        assertEquals("wall", onWall["face"])
        assertEquals("west", onWall["facing"], "on a wall it looks out of it")

        assertEquals("ceiling", placed("lever", face = Direction.DOWN)["face"])
    }

    /** parity: `AnvilBlock`, the one that lands across the player rather than facing them. */
    @Test
    fun anAnvilLandsSideways() {
        assertEquals("west", placed("anvil", yaw = 0f)["facing"])
    }

    /** Checked against the real classes: an amethyst bud grows out of the face it was put on. */
    @Test
    fun anAmethystBudPointsOutOfItsFace() {
        assertEquals("up", placed("small_amethyst_bud", face = Direction.UP)["facing"])
        assertEquals("north", placed("large_amethyst_bud", face = Direction.NORTH)["facing"])
    }

    /** parity: `DoorBlock`, which looks the same way the player does. */
    @Test
    fun aDoorFacesTheSameWayAsThePlayer() {
        assertEquals("south", placed("oak_door", yaw = 0f)["facing"])
    }

    /** The complaint from the screenshots: a block went down inside the player and walled them in. */
    @Test
    fun aBlockDoesNotGoDownInsideAPlayer() {
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)

        assertTrue(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 64, 0, listOf(standing)), "at the feet")
        assertTrue(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 65, 0, listOf(standing)), "at the head")
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 66, 0, listOf(standing)), "above the head")
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 1, 64, 0, listOf(standing)), "beside them")
    }

    /** parity: `Shapes.joinIsNotEmpty` with AND: touching is not overlapping. */
    @Test
    fun theBlockUnderfootIsNotInTheWay() {
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(Blocks.STONE.stateId, 0, 63, 0, listOf(standing)))
    }

    /** parity: 1.8 tall standing and 1.5 crouching, which is a real quarter of a block of room. */
    @Test
    fun crouchingIsShorterThanStanding() {
        val topSlab = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab"))) - 2
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        val crouching = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = true)

        // The upside-down slab fills 65.5 to 66.0: the standing head reaches it, the crouching one does not.
        assertTrue(BlockPlacement.isObstructed(topSlab, 0, 65, 0, listOf(standing)))
        assertFalse(BlockPlacement.isObstructed(topSlab, 0, 65, 0, listOf(crouching)))
    }

    /** parity: an empty collision shape never obstructs anyone, so a torch goes down inside you. */
    @Test
    fun somethingWithNoCollisionIsNeverInTheWay() {
        val torch = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("torch")))
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(torch, 0, 64, 0, listOf(standing)))
    }

    /** A bottom slab at the feet of someone standing on the ground does overlap them. */
    @Test
    fun aSlabAtTheFeetIsStillInTheWay() {
        val slab = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone_slab")))
        val standing = BlockPlacement.playerBox(x = 0.5, y = 64.0, z = 0.5, sneaking = false)
        assertTrue(BlockPlacement.isObstructed(slab, 0, 64, 0, listOf(standing)))
        // The top half of the same block is not: a bottom slab only fills the lower half.
        val onSlab = BlockPlacement.playerBox(x = 0.5, y = 64.5, z = 0.5, sneaking = false)
        assertFalse(BlockPlacement.isObstructed(slab, 0, 64, 0, listOf(onSlab)))
    }

    @Test
    fun aBlockWithNothingToOrientIsPlacedAsItIs() {
        val stone = Blocks.STONE.stateId
        assertEquals(
            stone,
            BlockPlacement.stateFor(
                Identifier.vanilla("stone"),
                stone,
                Direction.UP,
                clickY = 0f,
                yaw = 0f,
                pitch = 0f,
                replacing = Blocks.AIR.stateId,
            ),
        )
    }
}
