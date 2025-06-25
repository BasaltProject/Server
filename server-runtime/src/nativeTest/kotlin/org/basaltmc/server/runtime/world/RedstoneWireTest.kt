package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
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

/** parity: `RedStoneWireBlock.getConnectionState`, which is what draws a line of dust as a line. */
class RedstoneWireTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("wire", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/wire",
            Logger("RedstoneWireTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) world.state.setBlockState(x, 69, z, state("stone", emptyMap()))
        }
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String>,
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun dust(
        x: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            70,
            z,
            state(
                "redstone_wire",
                mapOf("north" to "none", "south" to "none", "east" to "none", "west" to "none", "power" to "0"),
            ),
        )
    }

    private fun sidesAt(
        x: Int,
        z: Int,
    ): Map<String, String> {
        val properties = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, 70, z) ?: 0)
        return listOf("north", "south", "east", "west").associateWith { properties[it] ?: "?" }
    }

    /**
     * parity: `getConnectionState`'s first clause. A dust with nothing around it stays a dot, which
     * is what it has looked like since the flattening: the cross is what a dust that lost its
     * neighbours turns back into.
     */
    @Test
    fun aLoneDustIsADot() {
        dust(2, 2)
        RedstoneWire.reshape(world, 2, 70, 2, checkNotNull(world.state.getBlockState(2, 70, 2)))

        assertEquals(mapOf("north" to "none", "south" to "none", "east" to "none", "west" to "none"), sidesAt(2, 2))
    }

    /** parity: a dust that had a connection and lost it straightens out instead of becoming a dot. */
    @Test
    fun aDustThatLosesItsNeighbourStraightensOut() {
        world.state.setBlockState(
            2,
            70,
            6,
            state(
                "redstone_wire",
                mapOf("north" to "side", "south" to "side", "east" to "none", "west" to "none", "power" to "0"),
            ),
        )

        RedstoneWire.reshape(world, 2, 70, 6, checkNotNull(world.state.getBlockState(2, 70, 6)))

        assertEquals(mapOf("north" to "side", "south" to "side", "east" to "side", "west" to "side"), sidesAt(2, 6))
    }

    /** Two dusts side by side make a line along that axis and nothing across it. */
    @Test
    fun twoDustsMakeALine() {
        dust(5, 2)
        dust(6, 2)
        for (x in 5..6) RedstoneWire.reshape(world, x, 70, 2, checkNotNull(world.state.getBlockState(x, 70, 2)))

        assertEquals("side", sidesAt(5, 2)["east"], "it reaches its neighbour")
        assertEquals("side", sidesAt(6, 2)["west"])
        assertEquals("none", sidesAt(5, 2)["north"], "and nothing across the line")
        assertEquals("none", sidesAt(6, 2)["south"])
    }

    /**
     * The path a real placement takes: the dust that is already there has to be redrawn by the
     * update the new one sends, not only the new one.
     */
    @Test
    fun dustAlreadyThereIsRedrawnWhenAnotherLandsBesideIt() {
        dust(5, 8)
        RedstoneWire.reshape(world, 5, 70, 8, checkNotNull(world.state.getBlockState(5, 70, 8)))
        assertEquals("none", sidesAt(5, 8)["east"], "alone, it is a dot")

        dust(6, 8)
        RedstoneWire.reshape(world, 6, 70, 8, checkNotNull(world.state.getBlockState(6, 70, 8)))
        NeighbourUpdates.afterChange(world, 6, 70, 8, reshapeSelf = false) { _, _, _, _ -> }

        assertEquals("side", sidesAt(5, 8)["east"], "and now it reaches the new one")
        assertEquals("side", sidesAt(6, 8)["west"])
    }

    /** parity: `RedstoneSide.UP`, the dust climbing the block beside it. */
    @Test
    fun aDustClimbsTheBlockBesideIt() {
        dust(9, 2)
        world.state.setBlockState(10, 70, 2, state("stone", emptyMap()))
        world.state.setBlockState(10, 71, 2, state("stone", emptyMap()))
        world.state.setBlockState(
            10,
            71,
            2,
            state(
                "redstone_wire",
                mapOf("north" to "none", "south" to "none", "east" to "none", "west" to "none", "power" to "0"),
            ),
        )

        RedstoneWire.reshape(world, 9, 70, 2, checkNotNull(world.state.getBlockState(9, 70, 2)))

        assertEquals("up", sidesAt(9, 2)["east"])
    }

    /** parity: `shouldConnectTo`, which reaches towards a lever but not towards plain stone. */
    @Test
    fun itReachesTowardsWhatCanPowerIt() {
        dust(13, 2)
        world.state.setBlockState(
            14,
            70,
            2,
            state("lever", mapOf("face" to "floor", "facing" to "north", "powered" to "false")),
        )
        world.state.setBlockState(13, 71, 2, state("stone", emptyMap()))

        RedstoneWire.reshape(world, 13, 70, 2, checkNotNull(world.state.getBlockState(13, 70, 2)))

        assertEquals("side", sidesAt(13, 2)["east"], "towards the lever")
    }
}
