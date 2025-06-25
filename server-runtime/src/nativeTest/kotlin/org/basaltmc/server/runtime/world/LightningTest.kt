package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `LightningBolt` and `LightningRodBlock.onLightningStrike`. */
class LightningTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("lightning", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/lightning",
            Logger("LightningTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun rod(
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
                    Identifier.vanilla("lightning_rod"),
                    mapOf("facing" to "up", "powered" to "false", "waterlogged" to "false"),
                ),
            ),
        )
    }

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    /** parity: `onLightningStrike`: the rod is powered, and books the eight ticks it holds for. */
    @Test
    fun aStrikePowersTheRodItHits() {
        rod(4, 70, 4)
        val changes = mutableListOf<BlockChange>()

        Lightning.strike(world, 4, 71, 4, lightFires = false, changes = changes)

        assertEquals("true", propertiesAt(4, 70, 4)["powered"])
        assertTrue(changes.isNotEmpty())
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(
            pending.any { it.localX == 4 && it.y == 70 && it.type.path == "lightning_rod" && it.delay == 8 },
            "eight ticks: $pending",
        )
    }

    /** parity: `LightningRodBlock.getSignal`, which is what the redstone beside it reads. */
    @Test
    fun aPoweredRodFeedsRedstone() {
        rod(6, 70, 6)
        Lightning.strike(world, 6, 71, 6, lightFires = false, changes = mutableListOf())

        val stateId = checkNotNull(world.state.getBlockState(6, 70, 6))
        assertEquals(15, Redstone.signalOf(stateId, BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(stateId, BlockFace.UP), "strongly the way it points")
    }

    @Test
    fun theScheduledTickLetsItGo() {
        rod(8, 70, 8)
        Lightning.strike(world, 8, 71, 8, lightFires = false, changes = mutableListOf())

        assertNotNull(Lightning.releaseRod(world, 8, 70, 8))

        assertEquals("false", propertiesAt(8, 70, 8)["powered"])
        assertNull(Lightning.releaseRod(world, 8, 70, 8), "and once dark it stays dark")
    }

    /** parity: `findLightningRod`, which pulls a strike to the nearest rod within 128 blocks. */
    @Test
    fun aRodPullsTheStrikeToItself() {
        for (x in 0 until 16) {
            for (z in 0 until 16) world.state.setBlockState(x, 64, z, Blocks.STONE.stateId)
        }
        rod(10, 65, 10)

        val target = assertNotNull(Lightning.targetFor(world, 2, 2))

        assertEquals(listOf(10, 66, 10), target.toList(), "the strike goes to the rod, not the ground")
    }

    /** With no rod anywhere, the strike lands on the top of the column it was aimed at. */
    @Test
    fun withoutARodItLandsWhereItWasAimed() {
        for (x in 0 until 16) {
            for (z in 0 until 16) world.state.setBlockState(x, 64, z, Blocks.STONE.stateId)
        }

        val target = assertNotNull(Lightning.targetFor(world, 3, 3))

        assertEquals(listOf(3, 65, 3), target.toList())
    }
}
