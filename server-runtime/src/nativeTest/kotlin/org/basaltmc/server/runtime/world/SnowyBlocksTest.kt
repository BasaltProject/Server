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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class SnowyBlocksTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("snow", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/snowy",
            logger = Logger("SnowyTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun grass(snowy: Boolean): Int =
        checkNotNull(
            BlockStateDefinitions.stateIdOf(Identifier.vanilla("grass_block"), mapOf("snowy" to snowy.toString())),
        )

    /** The bug from the screenshot: the snow went and the grass stayed white. */
    @Test
    fun breakingTheSnowTakesTheWhiteOffTheGrassUnderIt() {
        world.state.setBlockState(1, 64, 1, grass(snowy = true))
        world.state.setBlockState(1, 65, 1, Blocks.AIR.stateId)

        val updated = assertNotNull(SnowyBlocks.updated(world, 1, 64, 1))

        assertEquals("false", BlockStateDefinitions.propertiesOf(updated)["snowy"])
    }

    @Test
    fun puttingSnowOnGrassTurnsItWhite() {
        world.state.setBlockState(2, 64, 1, grass(snowy = false))
        world.state.setBlockState(2, 65, 1, Blocks.SNOW.stateId)

        val updated = assertNotNull(SnowyBlocks.updated(world, 2, 64, 1))

        assertEquals("true", BlockStateDefinitions.propertiesOf(updated)["snowy"])
    }

    /** parity: `BlockTags.SNOW`, which is snow, the snow block and powder snow, and nothing else. */
    @Test
    fun onlySnowCountsAsSnow() {
        assertTrue(SnowyBlocks.isSnow(Blocks.SNOW.stateId))
        assertTrue(SnowyBlocks.isSnow(Blocks.SNOW_BLOCK.stateId))
        assertTrue(SnowyBlocks.isSnow(checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("powder_snow")))))
        assertFalse(SnowyBlocks.isSnow(Blocks.ICE.stateId))
        assertFalse(SnowyBlocks.isSnow(Blocks.STONE.stateId))
    }

    @Test
    fun aBlockThatNeverWearsSnowIsLeftAlone() {
        world.state.setBlockState(3, 64, 1, Blocks.STONE.stateId)
        world.state.setBlockState(3, 65, 1, Blocks.SNOW.stateId)

        assertFalse(SnowyBlocks.wearsSnow(Blocks.STONE.stateId))
        assertNull(SnowyBlocks.updated(world, 3, 64, 1))
    }

    @Test
    fun grassThatAlreadyLooksRightIsNotRewritten() {
        world.state.setBlockState(4, 64, 1, grass(snowy = false))
        world.state.setBlockState(4, 65, 1, Blocks.AIR.stateId)

        assertNull(SnowyBlocks.updated(world, 4, 64, 1))
    }
}
