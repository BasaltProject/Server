package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
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
import kotlin.test.assertTrue

/** parity: `PoweredRailBlock.updateState` and its chain of eight. */
class PoweredRailsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("rails", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/rails",
            Logger("PoweredRailsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun poweredAt(z: Int): Boolean =
        BlockStateDefinitions.propertiesOf(world.state.getBlockState(1, 70, z) ?: 0)["powered"] == "true"

    /** A run of rails along z, with a block of redstone beside the first one. */
    private fun runOfRails(count: Int) {
        for (z in 0 until count) {
            world.state.setBlockState(
                1,
                70,
                z,
                state("powered_rail", mapOf("shape" to "north_south", "powered" to "false", "waterlogged" to "false")),
            )
        }
    }

    private fun settle(count: Int) {
        for (z in 0 until count) PoweredRails.update(world, 1, 70, z)
    }

    /** parity: the eighth rail along still lights, the ninth does not. */
    @Test
    fun oneSourceCarriesEightRailsAlong() {
        runOfRails(count = 11)
        world.state.setBlockState(0, 70, 0, state("redstone_block"))

        settle(11)

        for (z in 0..8) assertTrue(poweredAt(z), "rail $z is within the eight")
        assertFalse(poweredAt(9), "and the ninth is out of reach")
        assertFalse(poweredAt(10))
    }

    @Test
    fun takingThePowerAwayPutsThemAllOut() {
        runOfRails(count = 4)
        world.state.setBlockState(0, 70, 0, state("redstone_block"))
        settle(4)
        assertTrue(poweredAt(3))

        world.state.setBlockState(0, 70, 0, Blocks.AIR.stateId)
        // Furthest first, because each one leans on the one behind it, exactly as vanilla does.
        for (z in 3 downTo 0) PoweredRails.update(world, 1, 70, z)

        for (z in 0..3) assertFalse(poweredAt(z), "rail $z went out")
    }

    /** parity: `isSameRailWithPower`, which will not follow a rail that crosses the line. */
    @Test
    fun aRailAcrossTheLineStopsIt() {
        runOfRails(count = 4)
        world.state.setBlockState(
            1,
            70,
            1,
            state("powered_rail", mapOf("shape" to "east_west", "powered" to "false", "waterlogged" to "false")),
        )
        world.state.setBlockState(0, 70, 0, state("redstone_block"))

        settle(4)

        assertTrue(poweredAt(0), "the one being powered")
        assertFalse(poweredAt(2), "nothing gets past the one lying across")
        assertFalse(poweredAt(3))
    }

    /** An activator rail works the same way, and the two kinds do not feed each other. */
    @Test
    fun theTwoKindsDoNotShareTheirLine() {
        runOfRails(count = 3)
        world.state.setBlockState(
            1,
            70,
            1,
            state("activator_rail", mapOf("shape" to "north_south", "powered" to "false", "waterlogged" to "false")),
        )
        world.state.setBlockState(0, 70, 0, state("redstone_block"))

        settle(3)

        assertTrue(poweredAt(0))
        assertFalse(poweredAt(1), "an activator rail is not a powered rail")
        assertEquals(
            "activator_rail",
            BlockStateDefinitions.blockOf(world.state.getBlockState(1, 70, 1) ?: 0)?.path,
        )
        assertFalse(poweredAt(2))
    }
}
