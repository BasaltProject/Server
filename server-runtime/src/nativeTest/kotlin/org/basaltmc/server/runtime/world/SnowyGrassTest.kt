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
import kotlin.test.assertNull

/** parity: `SnowyBlock.updateShape`, the white top grass only wears while snow sits on it. */
class SnowyGrassTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("snowy", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/snowy-grass",
            Logger("SnowyGrassTest").apply { minLevel = LogLevel.ERROR },
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

    private fun snowyAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))["snowy"]

    @Test
    fun waterOverSnowyGrassTakesTheWhiteOff() {
        world.state.setBlockState(4, 70, 4, state("grass_block", mapOf("snowy" to "true")))
        world.state.setBlockState(4, 71, 4, state("snow", mapOf("layers" to "1")))
        assertEquals("true", snowyAt(4, 70, 4))

        // The water lands where the snow was, which is exactly what a bucket or a flow does.
        world.state.setBlockState(4, 71, 4, state("water", mapOf("level" to "0")))
        NeighbourUpdates.afterChange(world, 4, 71, 4, reshapeSelf = false, onBroken = { _, _, _, _ -> })

        assertEquals("false", snowyAt(4, 70, 4))
    }

    @Test
    fun airOverSnowyGrassTakesTheWhiteOffToo() {
        world.state.setBlockState(6, 70, 4, state("grass_block", mapOf("snowy" to "true")))
        world.state.setBlockState(6, 71, 4, state("snow", mapOf("layers" to "3")))
        world.state.setBlockState(6, 71, 4, state("air"))

        NeighbourUpdates.afterChange(world, 6, 71, 4, reshapeSelf = false, onBroken = { _, _, _, _ -> })

        assertEquals("false", snowyAt(6, 70, 4))
    }

    @Test
    fun grassUnderSnowKeepsWearingIt() {
        world.state.setBlockState(8, 70, 4, state("grass_block", mapOf("snowy" to "false")))
        world.state.setBlockState(8, 71, 4, state("snow_block"))

        NeighbourUpdates.afterChange(world, 8, 71, 4, reshapeSelf = false, onBroken = { _, _, _, _ -> })

        assertEquals("true", snowyAt(8, 70, 4))
    }

    @Test
    fun whatDoesNotWearSnowIsLeftAlone() {
        world.state.setBlockState(10, 70, 4, state("stone"))
        assertNull(SnowyBlocks.updated(world, 10, 70, 4))
    }
}
