package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `VibrationSystem` and `SculkSensorBlock`. */
class VibrationsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("sculk", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/sculk",
            Logger("VibrationsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun sensorAt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("sculk_sensor"),
                    mapOf("sculk_sensor_phase" to "inactive", "power" to "0", "waterlogged" to "false"),
                ),
            ),
        )
    }

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    /** parity: the frequency table, which is what a sensor's signal actually is. */
    @Test
    fun theFrequenciesAreVanillasOwn() {
        assertEquals(1, Vibrations.frequencyOf("step"))
        assertEquals(11, Vibrations.frequencyOf("block_change"))
        assertEquals(12, Vibrations.frequencyOf("block_destroy"))
        assertEquals(13, Vibrations.frequencyOf("block_place"))
        assertEquals(15, Vibrations.frequencyOf("explode"))
        assertEquals(0, Vibrations.frequencyOf("nothing_at_all"), "an event nothing listens to")
    }

    /** parity: `handleGameEvent`: the sensor lights with the event's own frequency. */
    @Test
    fun aSensorHearsWhatHappensNearIt() {
        sensorAt(4, 70, 4)
        val changes = mutableListOf<BlockChange>()

        Vibrations.emit(world, "block_place", 6.5, 70.5, 4.5, changes)

        assertEquals("active", propertiesAt(4, 70, 4)["sculk_sensor_phase"])
        assertEquals("13", propertiesAt(4, 70, 4)["power"], "a block placed is thirteen")
        assertTrue(changes.isNotEmpty())
    }

    /** parity: the eight blocks it listens over, and nothing beyond them. */
    @Test
    fun itHearsNothingTooFarAway() {
        sensorAt(6, 70, 6)

        Vibrations.emit(world, "explode", 20.5, 70.5, 6.5, mutableListOf())

        assertEquals("inactive", propertiesAt(6, 70, 6)["sculk_sensor_phase"])
    }

    /** parity: a sensor already awake hears nothing else until it has settled. */
    @Test
    fun oneAtATime() {
        sensorAt(8, 70, 8)
        Vibrations.emit(world, "step", 8.5, 70.5, 9.5, mutableListOf())
        assertEquals("1", propertiesAt(8, 70, 8)["power"])

        Vibrations.emit(world, "explode", 8.5, 70.5, 9.5, mutableListOf())

        assertEquals("1", propertiesAt(8, 70, 8)["power"], "the loud one has to wait its turn")
    }

    /** parity: `SculkSensorBlock.tick`: thirty ticks awake, ten of cooldown, then quiet. */
    @Test
    fun itGoesThroughItsPhases() {
        sensorAt(10, 70, 10)
        Vibrations.emit(world, "block_place", 10.5, 70.5, 11.5, mutableListOf())

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.localX == 10 && it.type.path == "sculk_sensor" && it.delay == 30 })

        assertNotNull(Vibrations.tick(world, 10, 70, 10))
        assertEquals("cooldown", propertiesAt(10, 70, 10)["sculk_sensor_phase"])
        assertEquals("0", propertiesAt(10, 70, 10)["power"], "and it gives up its signal at once")

        assertNotNull(Vibrations.tick(world, 10, 70, 10))
        assertEquals("inactive", propertiesAt(10, 70, 10)["sculk_sensor_phase"])
        assertNull(Vibrations.tick(world, 10, 70, 10), "and there is nothing left to do")
    }

    /** parity: `SculkSensorBlock.getSignal`, which is what the redstone beside it reads. */
    @Test
    fun anActiveSensorFeedsRedstone() {
        sensorAt(12, 70, 12)
        Vibrations.emit(world, "container_open", 12.5, 70.5, 13.5, mutableListOf())

        val stateId = checkNotNull(world.state.getBlockState(12, 70, 12))
        assertEquals(10, Redstone.signalOf(stateId, BlockFace.NORTH), "a container opening is ten")
        assertEquals(10, Redstone.directSignalOf(stateId, BlockFace.UP), "strongly downwards only")
        assertEquals(0, Redstone.directSignalOf(stateId, BlockFace.NORTH))
    }

    /** The index a footstep asks before it looks for anything, so walking costs nothing. */
    @Test
    fun theWorldRemembersWhereItsSensorsAre() {
        assertTrue(!world.state.hasSculkSensorNear(14.5, 70.0, 14.5, Vibrations.RANGE), "none yet")

        sensorAt(14, 70, 14)

        assertTrue(world.state.hasSculkSensorNear(14.5, 70.0, 14.5, Vibrations.RANGE))
        assertTrue(
            !world.state.hasSculkSensorNear(40.5, 70.0, 40.5, Vibrations.RANGE),
            "and it is only near the ones it is near",
        )

        world.state.setBlockState(14, 70, 14, org.basaltmc.server.registry.Blocks.AIR.stateId)
        assertTrue(!world.state.hasSculkSensorNear(14.5, 70.0, 14.5, Vibrations.RANGE), "broken, so forgotten")
    }

    /** parity: `GameEvent.STEP` is one, the quietest thing a sensor answers to. */
    @Test
    fun aFootstepIsTheQuietestVibration() {
        sensorAt(15, 70, 15)

        Vibrations.emit(world, "step", 14.5, 70.5, 15.5, mutableListOf())

        assertEquals("1", propertiesAt(15, 70, 15)["power"])
    }
}
