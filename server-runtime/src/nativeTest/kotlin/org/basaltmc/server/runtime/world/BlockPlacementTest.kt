package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Direction
import kotlin.test.Test
import kotlin.test.assertEquals

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
