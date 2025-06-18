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

class ExplosionsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("boom", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/boom",
            Logger("BoomTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun fill(
        block: Int,
        from: Int,
        to: Int,
    ) {
        for (x in from..to) {
            for (y in from..to) {
                for (z in from..to) world.state.setBlockState(x, y + 60, z, block)
            }
        }
    }

    /** parity: `ServerExplosion`, which clears a rough sphere out of soft ground. */
    @Test
    fun anExplosionClearsAHoleInDirt() {
        fill(Blocks.DIRT.stateId, 0, 12)

        val broken = Explosions.explode(world, 6.5, 66.5, 6.5, Explosions.BED_POWER, LegacyRandom(9L))

        assertTrue(broken.size > 40, "a five-power blast should take out dozens of blocks, got ${broken.size}")
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(6, 66, 6), "the middle should be gone")
        assertTrue(
            broken.all {
                kotlin.math.abs(it.x - 6) <= 6 && kotlin.math.abs(it.y - 66) <= 6 && kotlin.math.abs(it.z - 6) <= 6
            },
            "and nothing far outside the blast",
        )
    }

    /** parity: `Block.getExplosionResistance`: obsidian shrugs a bed off, bedrock cannot be broken. */
    @Test
    fun obsidianAndBedrockSurviveWhatDirtDoesNot() {
        fill(checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("obsidian"), emptyMap())), 0, 12)
        world.state.setBlockState(6, 66, 6, Blocks.AIR.stateId)

        val broken = Explosions.explode(world, 6.5, 66.5, 6.5, Explosions.BED_POWER, LegacyRandom(9L))

        assertTrue(broken.isEmpty(), "obsidian should have held, got ${broken.size} blocks")
    }

    @Test
    fun theDamageFallsOffWithDistance() {
        val close = Explosions.damageAt(0.5, Explosions.BED_POWER, Explosions.BED_POWER * 2f)
        val far = Explosions.damageAt(9.0, Explosions.BED_POWER, Explosions.BED_POWER * 2f)

        assertTrue(close > far, "closer should hurt more: $close against $far")
        assertEquals(0f, Explosions.damageAt(20.0, Explosions.BED_POWER, Explosions.BED_POWER * 2f))
    }
}
