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
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `BasePressurePlateBlock` and the two kinds of plate. */
class PressurePlatesTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("plates", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/plates",
            Logger("PressurePlatesTest").apply { minLevel = LogLevel.ERROR },
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

    private fun propertiesAt(): Map<String, String> =
        BlockStateDefinitions.propertiesOf(world.state.getBlockState(4, 70, 4) ?: 0)

    /** parity: standing on one presses it, and it books the twenty ticks that let it up again. */
    @Test
    fun standingOnOnePressesIt() {
        world.state.setBlockState(4, 70, 4, state("oak_pressure_plate", mapOf("powered" to "false")))

        assertNotNull(PressurePlates.update(world, 4, 70, 4, standing = 1))
        assertEquals("true", propertiesAt()["powered"])
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.y == 70 && it.delay == 20 }, "twenty ticks, like vanilla: $pending")

        assertNull(PressurePlates.update(world, 4, 70, 4, standing = 2), "already pressed")

        assertNotNull(PressurePlates.update(world, 4, 70, 4, standing = 0))
        assertEquals("false", propertiesAt()["powered"], "and nobody on it lets it up")
    }

    /** parity: a plate feeds everything beside it and strongly powers only the block below. */
    @Test
    fun aPressedPlateFeedsTheBlockUnderIt() {
        world.state.setBlockState(4, 70, 4, state("oak_pressure_plate", mapOf("powered" to "false")))
        PressurePlates.update(world, 4, 70, 4, standing = 1)
        val pressed = checkNotNull(world.state.getBlockState(4, 70, 4))

        assertEquals(15, Redstone.signalOf(pressed, org.basaltmc.server.registry.BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(pressed, org.basaltmc.server.registry.BlockFace.UP))
        assertEquals(0, Redstone.directSignalOf(pressed, org.basaltmc.server.registry.BlockFace.NORTH))
    }

    /**
     * parity: `WeightedPressurePlateBlock.getSignalStrength`, which scales against its own limit and
     * rounds up: one item on a gold plate already reads one, fifteen read fifteen.
     */
    @Test
    fun aWeightedPlateCountsWhatIsOnIt() {
        world.state.setBlockState(4, 70, 4, state("light_weighted_pressure_plate", mapOf("power" to "0")))

        PressurePlates.update(world, 4, 70, 4, standing = 1)
        assertEquals("1", propertiesAt()["power"])

        PressurePlates.update(world, 4, 70, 4, standing = 8)
        assertEquals("8", propertiesAt()["power"])

        PressurePlates.update(world, 4, 70, 4, standing = 40)
        assertEquals("15", propertiesAt()["power"], "its limit is fifteen and it stops there")

        // parity: `getPressedTime`, ten ticks for a weighted plate rather than twenty.
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.y == 70 && it.delay == 10 }, "ten ticks: $pending")
    }

    /** parity: `TOUCH_AABB`, narrower than the block: the very edge does not press it. */
    @Test
    fun theEdgeDoesNotCount() {
        assertTrue(PressurePlates.isStandingOn(4, 70, 4, 4.5, 70.0, 4.5), "standing in the middle")
        assertTrue(PressurePlates.isStandingOn(4, 70, 4, 4.5, 70.2, 4.5), "and a little above it")
        assertFalse(PressurePlates.isStandingOn(4, 70, 4, 4.5, 70.5, 4.5), "but not halfway up the block")
        assertFalse(PressurePlates.isStandingOn(4, 70, 4, 4.01, 70.0, 4.5), "nor along the very edge")
        assertFalse(PressurePlates.isStandingOn(4, 70, 4, 4.5, 70.0, 5.5), "nor in the next block")
    }
}
