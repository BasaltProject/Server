package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `DefaultRedstoneWireEvaluator` and `Level.getBestNeighborSignal`. */
class RedstoneTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("redstone", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/redstone",
            Logger("RedstoneTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
        // A line of dust long enough to fade out runs past x=15.
        world.state.getOrGenerateChunk(1, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun powerAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["power"]?.toIntOrNull()

    /** A line of dust on the floor, starting at x and running east. */
    private fun dustLine(
        x: Int,
        y: Int,
        z: Int,
        length: Int,
    ) {
        for (step in 0 until length) {
            world.state.setBlockState(x + step, y - 1, z, Blocks.STONE.stateId)
            world.state.setBlockState(x + step, y, z, state("redstone_wire", mapOf("power" to "0")))
        }
    }

    @Test
    fun aBlockOfRedstoneFeedsFifteenAndTheDustFadesByOne() {
        world.state.setBlockState(1, 70, 1, state("redstone_block"))
        dustLine(2, 70, 1, length = 5)

        Redstone.settle(world, 2, 70, 1)

        assertEquals(15, powerAt(2, 70, 1), "the piece touching the block gets it all")
        assertEquals(14, powerAt(3, 70, 1))
        assertEquals(13, powerAt(4, 70, 1))
        assertEquals(12, powerAt(5, 70, 1))
        assertEquals(11, powerAt(6, 70, 1))
    }

    /** parity: fifteen pieces of dust and the sixteenth is dark. */
    @Test
    fun theSignalDiesAfterFifteenBlocks() {
        world.state.setBlockState(1, 70, 5, state("redstone_block"))
        dustLine(2, 70, 5, length = 17)

        Redstone.settle(world, 2, 70, 5)

        assertEquals(1, powerAt(16, 70, 5), "the fifteenth piece still has one")
        assertEquals(0, powerAt(17, 70, 5), "and the sixteenth has nothing")
    }

    /** parity: `getSignal`, which a lever gives only while it is flipped. */
    @Test
    fun aLeverFeedsTheDustBesideIt() {
        world.state.setBlockState(1, 70, 8, Blocks.STONE.stateId)
        dustLine(2, 70, 8, length = 3)

        val off = state("lever", mapOf("face" to "floor", "facing" to "north", "powered" to "false"))
        world.state.setBlockState(1, 71, 8, off)
        Redstone.settle(world, 2, 70, 8)
        assertEquals(0, powerAt(2, 70, 8), "an unflipped lever feeds nothing")

        // parity: a lever strongly powers the block it is attached to, and dust beside *that* block
        // reads it. It is how half of redstone is switched on.
        val on = state("lever", mapOf("face" to "floor", "facing" to "north", "powered" to "true"))
        world.state.setBlockState(1, 71, 8, on)
        Redstone.settle(world, 2, 70, 8)
        assertEquals(15, powerAt(2, 70, 8), "through the block it is standing on")

        world.state.setBlockState(1, 71, 8, off)
        Redstone.settle(world, 2, 70, 8)
        assertEquals(0, powerAt(2, 70, 8), "and back to nothing when it is flicked off")
    }

    /** parity: `getIncomingWireSignal`: dust climbs a block that conducts and drops off one that does not. */
    @Test
    fun dustClimbsAndFalls() {
        world.state.setBlockState(1, 70, 12, state("redstone_block"))
        world.state.setBlockState(2, 69, 12, Blocks.STONE.stateId)
        world.state.setBlockState(2, 70, 12, state("redstone_wire", mapOf("power" to "0")))
        // A step up onto a stone block, with nothing over the corner so the dust can see it.
        world.state.setBlockState(3, 69, 12, Blocks.STONE.stateId)
        world.state.setBlockState(3, 70, 12, Blocks.STONE.stateId)
        world.state.setBlockState(3, 71, 12, state("redstone_wire", mapOf("power" to "0")))

        Redstone.settle(world, 2, 70, 12)

        assertEquals(15, powerAt(2, 70, 12))
        assertEquals(14, powerAt(3, 71, 12), "the dust on top of the step still hears it")
    }

    /** parity: `RedstoneTorchBlock.getSignal`: a torch feeds every side but the one above it. */
    @Test
    fun aTorchFeedsTheDustBesideItAndTheBlockOverIt() {
        val litTorch = state("redstone_torch", mapOf("lit" to "true"))
        assertEquals(15, Redstone.signalOf(litTorch, org.basaltmc.server.registry.BlockFace.DOWN))
        assertEquals(15, Redstone.signalOf(litTorch, org.basaltmc.server.registry.BlockFace.NORTH))
        assertEquals(
            0,
            Redstone.signalOf(litTorch, org.basaltmc.server.registry.BlockFace.UP),
            "asked from below, which is the block the torch stands on",
        )
    }

    /**
     * parity: `RedstoneTorchBlock.tick` over `hasNeighborSignal`, which asks the block below with
     * `getSignal`: a torch goes out when what it stands on is *strongly* powered, which is what a
     * lever on that block does. A block of redstone beside it would not: that power is weak.
     */
    @Test
    fun aTorchGoesOutOnAStronglyPoweredBlock() {
        world.state.setBlockState(6, 70, 3, Blocks.STONE.stateId)
        world.state.setBlockState(6, 71, 3, state("redstone_torch", mapOf("lit" to "true")))
        world.state.setBlockState(5, 70, 3, state("redstone_block"))

        assertNull(RedstoneComponents.tick(world, 6, 71, 3), "a block of redstone only powers weakly")

        // A lever on the side of that same stone powers it strongly.
        world.state.setBlockState(
            7,
            70,
            3,
            state("lever", mapOf("face" to "wall", "facing" to "east", "powered" to "true")),
        )
        val out = checkNotNull(RedstoneComponents.tick(world, 6, 71, 3))
        assertEquals("false", BlockStateDefinitions.propertiesOf(out.stateId)["lit"], "and a lever does")

        world.state.setBlockState(7, 70, 3, Blocks.AIR.stateId)
        val back = checkNotNull(RedstoneComponents.tick(world, 6, 71, 3))
        assertEquals("true", BlockStateDefinitions.propertiesOf(back.stateId)["lit"], "then it lights again")
    }

    /** parity: `DiodeBlock.tick`: a repeater reads the block it faces and feeds the other side. */
    @Test
    fun aRepeaterPassesTheSignalOnItsOwnTurn() {
        world.state.setBlockState(9, 70, 3, state("redstone_block"))
        // Facing west, so its input is the block of redstone at x=9 and its output is eastwards.
        val repeater =
            state("repeater", mapOf("facing" to "west", "delay" to "1", "powered" to "false", "locked" to "false"))
        world.state.setBlockState(10, 70, 3, repeater)

        val turned = checkNotNull(RedstoneComponents.tick(world, 10, 70, 3))

        assertEquals("true", BlockStateDefinitions.propertiesOf(turned.stateId)["powered"])
        assertEquals(
            15,
            Redstone.signalOf(turned.stateId, org.basaltmc.server.registry.BlockFace.WEST),
            "which is what the block on its output side sees",
        )
    }

    /** A repeater books its turn for its own delay: two ticks per notch. */
    @Test
    fun aRepeaterBooksItsDelay() {
        world.state.setBlockState(12, 70, 3, state("redstone_block"))
        world.state.setBlockState(
            13,
            70,
            3,
            state("repeater", mapOf("facing" to "west", "delay" to "3", "powered" to "false", "locked" to "false")),
        )

        RedstoneComponents.scheduleAround(world, 12, 70, 3)

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        val due = checkNotNull(pending.firstOrNull { it.localX == 13 && it.y == 70 })
        assertEquals(6, due.delay, "three notches, two ticks each")
    }

    /** The table the signal walks over: a slab conducts, glass does not. */
    @Test
    fun whatCountsAsSolidForRedstone() {
        assertTrue(BlockSupport.conductsRedstone(Blocks.STONE.stateId))
        assertTrue(
            BlockSupport.conductsRedstone(state("oak_slab", mapOf("type" to "double", "waterlogged" to "false"))),
        )
        assertFalse(BlockSupport.conductsRedstone(state("glass")))
        assertFalse(BlockSupport.conductsRedstone(Blocks.AIR.stateId))
    }
}
