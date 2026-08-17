package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
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
import kotlin.test.assertTrue

/** parity: `SnifferEggBlock`, which cracks twice on a clock and then goes. */
class SnifferEggTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("sniffer", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/sniffer",
            Logger("SnifferEggTest").apply { minLevel = LogLevel.ERROR },
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

    private fun hatchAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["hatch"]?.toIntOrNull()

    private fun tick(
        x: Int,
        y: Int,
        z: Int,
    ): List<BlockChange> {
        val changes = mutableListOf<BlockChange>()
        ScheduledBlocks.snifferEgg(world, x, y, z, LegacyRandom(1L), changes)
        return changes
    }

    @Test
    fun itCracksTwiceAndThenGoes() {
        world.state.setBlockState(1, 70, 1, state("sniffer_egg", mapOf("hatch" to "0")))

        tick(1, 70, 1)
        assertEquals(1, hatchAt(1, 70, 1))
        tick(1, 70, 1)
        assertEquals(2, hatchAt(1, 70, 1))
        tick(1, 70, 1)
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(1, 70, 1), "the egg hatches away")
    }

    /** parity: `SnifferEggBlock.onPlace`, which asks for its first turn as the egg lands. */
    @Test
    fun placingOneAsksForATurn() {
        world.state.setBlockState(2, 70, 1, state("sniffer_egg", mapOf("hatch" to "0")))
        NeighbourUpdates.afterChange(world, 2, 70, 1) { _, _, _, _ -> }

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.type.path == "sniffer_egg" && it.localX == 2 && it.y == 70 && it.localZ == 1 },
            "no hatch scheduled: $pending",
        )
    }

    /**
     * parity: `SnifferEggBlock.hatchBoost` over `#minecraft:sniffer_egg_hatch_boost`, which is moss
     * and nothing else: an egg on it waits half as long.
     */
    @Test
    fun mossHalvesTheWait() {
        world.state.setBlockState(4, 69, 1, state("moss_block"))
        world.state.setBlockState(4, 70, 1, state("sniffer_egg", mapOf("hatch" to "0")))
        world.state.setBlockState(6, 70, 1, state("sniffer_egg", mapOf("hatch" to "0")))

        ScheduledBlocks.scheduleHatch(world, 4, 70, 1, seed = 0L)
        ScheduledBlocks.scheduleHatch(world, 6, 70, 1, seed = 0L)

        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        val onMoss = checkNotNull(pending.firstOrNull { it.localX == 4 && it.y == 70 })
        val onNothing = checkNotNull(pending.firstOrNull { it.localX == 6 && it.y == 70 })
        assertEquals(
            4000,
            onNothing.delay - onMoss.delay,
            "moss halves the wait, so a third of it is four thousand ticks",
        )
    }
}
