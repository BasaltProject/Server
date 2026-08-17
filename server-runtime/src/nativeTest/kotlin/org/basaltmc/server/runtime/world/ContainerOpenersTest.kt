package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `ContainerOpenersCounter` and `TrappedChestBlock.getSignal`. */
class ContainerOpenersTest {
    @AfterTest
    fun forgetEveryone() {
        ContainerOpeners.clear()
    }

    @Test
    fun itCountsWhoIsLookingPerWorldAndPlace() {
        assertEquals(0, ContainerOpeners.count("overworld", 1, 70, 1))

        assertEquals(1, ContainerOpeners.open("overworld", 1, 70, 1))
        assertEquals(2, ContainerOpeners.open("overworld", 1, 70, 1))
        assertEquals(0, ContainerOpeners.count("nether", 1, 70, 1), "another world is its own")
        assertEquals(0, ContainerOpeners.count("overworld", 1, 71, 1), "and so is another block")

        assertEquals(1, ContainerOpeners.close("overworld", 1, 70, 1))
        assertEquals(0, ContainerOpeners.close("overworld", 1, 70, 1))
        assertEquals(0, ContainerOpeners.close("overworld", 1, 70, 1), "closing what is shut stays at zero")
    }

    /** parity: `TrappedChestBlock`: one per person inside, and it only feeds the block below it. */
    @Test
    fun aTrappedChestGivesOffOnePerPerson() {
        val chest =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(Identifier.vanilla("trapped_chest"), mapOf("type" to "single")),
            )

        assertEquals(0, Redstone.signalOf(chest, BlockFace.UP, openers = 0))
        assertEquals(2, Redstone.signalOf(chest, BlockFace.UP, openers = 2))
        assertEquals(15, Redstone.signalOf(chest, BlockFace.NORTH, openers = 40), "fifteen is as far as it goes")

        assertEquals(1, Redstone.directSignalOf(chest, BlockFace.UP, openers = 1), "strongly, downwards only")
        assertEquals(0, Redstone.directSignalOf(chest, BlockFace.NORTH, openers = 1))
    }

    /** parity: the sources whose strength is a property, whatever it is that sets it. */
    @Test
    fun theOtherSourcesReadTheirOwnState() {
        val rail =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("detector_rail"),
                    mapOf(
                        "powered" to "true",
                    ),
                ),
            )
        assertEquals(15, Redstone.signalOf(rail, BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(rail, BlockFace.UP), "and downwards only, strongly")
        assertEquals(0, Redstone.directSignalOf(rail, BlockFace.NORTH))

        val target = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("target"), mapOf("power" to "9")))
        assertEquals(9, Redstone.signalOf(target, BlockFace.SOUTH))

        val hook =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("tripwire_hook"),
                    mapOf("powered" to "true", "facing" to "east"),
                ),
            )
        assertEquals(15, Redstone.signalOf(hook, BlockFace.WEST))
        assertEquals(15, Redstone.directSignalOf(hook, BlockFace.EAST), "strongly into the block it faces")
        assertEquals(0, Redstone.directSignalOf(hook, BlockFace.WEST))
    }
}
