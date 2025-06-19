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
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: the `neighborChanged` of every block that reads the power around it. */
class RedstoneReactorsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("reactors", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/reactors",
            Logger("RedstoneReactorsTest").apply { minLevel = LogLevel.ERROR },
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

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ): Map<String, String> = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    /** parity: `DoorBlock.neighborChanged`, which listens at both halves. */
    @Test
    fun aDoorOpensOnASignalAtEitherHalf() {
        world.state.setBlockState(
            2,
            70,
            2,
            state("oak_door", mapOf("half" to "lower", "facing" to "north", "open" to "false", "powered" to "false")),
        )
        world.state.setBlockState(
            2,
            71,
            2,
            state("oak_door", mapOf("half" to "upper", "facing" to "north", "open" to "false", "powered" to "false")),
        )

        // A block of redstone beside the *upper* half opens the whole door.
        world.state.setBlockState(3, 71, 2, state("redstone_block"))
        RedstoneReactors.update(world, 3, 71, 2)

        assertEquals("true", propertiesAt(2, 71, 2)["open"], "the half beside the signal")
        assertEquals("true", propertiesAt(2, 70, 2)["open"], "and the other one, which heard it through its twin")

        world.state.setBlockState(3, 71, 2, Blocks.AIR.stateId)
        RedstoneReactors.update(world, 3, 71, 2)
        assertEquals("false", propertiesAt(2, 70, 2)["open"], "and it closes again")
    }

    @Test
    fun aTrapdoorAndAGateFollowTheSignal() {
        world.state.setBlockState(
            5,
            70,
            2,
            state(
                "oak_trapdoor",
                mapOf("facing" to "north", "half" to "bottom", "open" to "false", "powered" to "false"),
            ),
        )
        world.state.setBlockState(
            7,
            70,
            2,
            state(
                "oak_fence_gate",
                mapOf(
                    "facing" to "north",
                    "open" to "false",
                    "powered" to "false",
                ),
            ),
        )
        world.state.setBlockState(5, 71, 2, state("redstone_block"))
        world.state.setBlockState(7, 71, 2, state("redstone_block"))

        RedstoneReactors.update(world, 5, 71, 2)
        RedstoneReactors.update(world, 7, 71, 2)

        assertEquals("true", propertiesAt(5, 70, 2)["open"])
        assertEquals("true", propertiesAt(7, 70, 2)["open"])
    }

    /** parity: `RedstoneLampBlock`: on at once, off four ticks later, so a blink does not reach it. */
    @Test
    fun aLampLightsAtOnceAndTakesFourTicksToGoOut() {
        world.state.setBlockState(9, 70, 2, state("redstone_lamp", mapOf("lit" to "false")))
        world.state.setBlockState(9, 71, 2, state("redstone_block"))

        RedstoneReactors.update(world, 9, 71, 2)
        assertEquals("true", propertiesAt(9, 70, 2)["lit"], "it lights the moment the signal arrives")

        world.state.setBlockState(9, 71, 2, Blocks.AIR.stateId)
        RedstoneReactors.update(world, 9, 71, 2)
        assertEquals("true", propertiesAt(9, 70, 2)["lit"], "and stays lit while its four ticks run")

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.localX == 9 && it.y == 70 && it.delay == 4 })
        assertEquals(
            "false",
            BlockStateDefinitions.propertiesOf(checkNotNull(RedstoneReactors.lampTick(world, 9, 70, 2)).stateId)["lit"],
        )
    }

    /** parity: `CopperBulbBlock.checkAndFlip`: it flips its light on the rising edge only. */
    @Test
    fun aCopperBulbIsAFlipFlop() {
        world.state.setBlockState(12, 70, 2, state("copper_bulb", mapOf("lit" to "false", "powered" to "false")))

        world.state.setBlockState(12, 71, 2, state("redstone_block"))
        RedstoneReactors.update(world, 12, 71, 2)
        assertEquals("true", propertiesAt(12, 70, 2)["lit"], "the rising edge turns it on")

        world.state.setBlockState(12, 71, 2, Blocks.AIR.stateId)
        RedstoneReactors.update(world, 12, 71, 2)
        assertEquals("true", propertiesAt(12, 70, 2)["lit"], "and losing the signal leaves it on")

        world.state.setBlockState(12, 71, 2, state("redstone_block"))
        RedstoneReactors.update(world, 12, 71, 2)
        assertEquals("false", propertiesAt(12, 70, 2)["lit"], "the next signal turns it off")
    }

    /** parity: `TntBlock.neighborChanged`, which primes rather than explodes on the spot. */
    @Test
    fun tntTakesItsFuse() {
        world.state.setBlockState(14, 70, 2, state("tnt", mapOf("unstable" to "false")))
        world.state.setBlockState(14, 71, 2, state("redstone_block"))

        RedstoneReactors.update(world, 14, 71, 2)

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.localX == 14 && it.y == 70 && it.type.path == "tnt" && it.delay == 80 },
            "eighty ticks, which is vanilla's fuse: $pending",
        )
    }

    /** A block nobody powered stays where it is. */
    @Test
    fun nothingHappensWithoutASignal() {
        world.state.setBlockState(
            0,
            70,
            5,
            state(
                "oak_trapdoor",
                mapOf("facing" to "north", "half" to "bottom", "open" to "false", "powered" to "false"),
            ),
        )

        assertEquals(0, RedstoneReactors.update(world, 0, 70, 5).size)
        assertNull(propertiesAt(0, 70, 5)["open"]?.takeIf { it == "true" })
    }
}
