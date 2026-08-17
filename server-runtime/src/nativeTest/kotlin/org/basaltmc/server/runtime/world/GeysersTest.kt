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

/** parity: `PotentSulfurBlock.validBlockState` and the countdown its block entity keeps. */
class GeysersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("geysers", WorldDimension.OVERWORLD, WorldType.VOID, seed = 42L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(42L, 0, 64, 0),
            "build/test-data/geysers",
            Logger("GeysersTest").apply { minLevel = LogLevel.ERROR },
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

    private fun stateAt(
        x: Int,
        z: Int,
    ): String? = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, 70, z) ?: 0)["potent_sulfur_state"]

    /** Sulfur at y=70 with [below] under it and [water] blocks of still water on top. */
    private fun geyser(
        x: Int,
        below: Int,
        water: Int = 2,
        state: String = "dry",
    ) {
        world.state.setBlockState(x, 69, 1, below)
        world.state.setBlockState(x, 70, 1, state("potent_sulfur", mapOf("potent_sulfur_state" to state)))
        for (y in 71 until 71 + water) world.state.setBlockState(x, y, 1, Blocks.WATER.stateId)
    }

    @Test
    fun withoutWaterOnTopItIsDry() {
        geyser(1, below = state("magma_block"), water = 0, state = "wet")

        Geysers.validState(world, 1, 70, 1, checkNotNull(world.state.getBlockState(1, 70, 1)))

        assertEquals("dry", stateAt(1, 1))
    }

    @Test
    fun waterAndPlainStoneMakeItWet() {
        geyser(3, below = Blocks.STONE.stateId)

        Geysers.validState(world, 3, 70, 1, checkNotNull(world.state.getBlockState(3, 70, 1)))

        assertEquals("wet", stateAt(3, 1))
    }

    /** parity: `#minecraft:causes_continuous_geyser_eruptions`, which is lava and nothing else. */
    @Test
    fun lavaUnderneathMakesItEruptWithoutStopping() {
        geyser(5, below = Blocks.LAVA.stateId)

        Geysers.validState(world, 5, 70, 1, checkNotNull(world.state.getBlockState(5, 70, 1)))

        assertEquals("continuous", stateAt(5, 1))
    }

    /** parity: `#minecraft:causes_periodic_geyser_eruptions`: magma, and it starts out waiting. */
    @Test
    fun magmaUnderneathMakesItWaitAndThenErupt() {
        geyser(7, below = state("magma_block"))

        Geysers.validState(world, 7, 70, 1, checkNotNull(world.state.getBlockState(7, 70, 1)))
        assertEquals("dormant", stateAt(7, 1))

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        val due = checkNotNull(pending.firstOrNull { it.localX == 7 && it.y == 70 })
        assertTrue(due.delay % 20 == 0, "the turn has to land on a twenty-tick boundary: ${due.delay}")

        val changes = mutableListOf<BlockChange>()
        Geysers.tick(world, 7, 70, 1, changes)
        assertEquals("erupting", stateAt(7, 1), "the wait is over")

        Geysers.tick(world, 7, 70, 1, changes)
        assertEquals("dormant", stateAt(7, 1), "and it goes back to waiting")
    }

    /** The wait is forked from the seed at the position, so it is the same every run and per block. */
    @Test
    fun twoGeysersKeepTheirOwnRhythm() {
        geyser(9, below = state("magma_block"))
        geyser(11, below = state("magma_block"))

        Geysers.scheduleTurn(world, 9, 70, 1, "dormant")
        Geysers.scheduleTurn(world, 11, 70, 1, "dormant")

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        val first = checkNotNull(pending.firstOrNull { it.localX == 9 && it.y == 70 })
        val second = checkNotNull(pending.firstOrNull { it.localX == 11 && it.y == 70 })
        assertTrue(first.delay != second.delay, "both waited exactly the same: ${first.delay}")
    }

    /** parity: `MossyCarpetBlock.getUpdatedState`: a side with no wall behind it goes, and with it the carpet. */
    @Test
    fun paleMossCarpetDropsASideItCannotHold() {
        world.state.setBlockState(11, 69, 3, Blocks.STONE.stateId)
        world.state.setBlockState(12, 70, 3, Blocks.STONE.stateId)
        world.state.setBlockState(
            11,
            70,
            3,
            state("pale_moss_carpet", mapOf("bottom" to "false", "east" to "low")),
        )

        assertEquals("low", BlockStateDefinitions.propertiesOf(world.state.getBlockState(11, 70, 3) ?: 0)["east"])

        world.state.setBlockState(12, 70, 3, Blocks.AIR.stateId)
        NeighbourUpdates.afterChange(world, 12, 70, 3) { _, _, _, _ -> }

        assertEquals(
            Blocks.AIR.stateId,
            world.state.getBlockState(11, 70, 3),
            "with no wall and no base there is nothing left of it",
        )
    }

    /** parity: `SulfurSpikeBlock` is a speleothem too, so its thickness follows the same rule. */
    @Test
    fun aSulfurSpikeTakesItsThickness() {
        world.state.setBlockState(13, 71, 3, state("sulfur"))
        world.state.setBlockState(
            13,
            70,
            3,
            state(
                "sulfur_spike",
                mapOf("vertical_direction" to "down", "thickness" to "tip", "waterlogged" to "false"),
            ),
        )
        world.state.setBlockState(
            13,
            69,
            3,
            state(
                "sulfur_spike",
                mapOf("vertical_direction" to "down", "thickness" to "tip", "waterlogged" to "false"),
            ),
        )

        NeighbourUpdates.afterChange(world, 13, 69, 3) { _, _, _, _ -> }

        // parity: a piece hanging off a tip is a frustum; base is for one with something thicker
        // than a tip below it.
        assertEquals(
            "frustum",
            BlockStateDefinitions.propertiesOf(world.state.getBlockState(13, 70, 3) ?: 0)["thickness"],
        )
        assertEquals(
            "tip",
            BlockStateDefinitions.propertiesOf(world.state.getBlockState(13, 69, 3) ?: 0)["thickness"],
            "and the one hanging off it stays a tip",
        )
    }

    /** A geyser capped by something solid has nowhere to send its plume, so it keeps no clock. */
    @Test
    fun aCappedGeyserSchedulesNothing() {
        geyser(13, below = state("magma_block"), water = 1)
        world.state.setBlockState(13, 72, 1, Blocks.STONE.stateId)

        Geysers.scheduleTurn(world, 13, 70, 1, "dormant")

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertNull(pending.firstOrNull { it.localX == 13 && it.y == 70 })
    }
}
