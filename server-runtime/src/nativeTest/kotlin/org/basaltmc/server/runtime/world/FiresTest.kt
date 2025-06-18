package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.FireOdds
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

class FiresTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("fire", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/fire",
            Logger("FireTest").apply { minLevel = LogLevel.ERROR },
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

    private fun burn(
        x: Int,
        y: Int,
        z: Int,
        rounds: Int = 400,
        until: () -> Boolean,
    ): Boolean {
        val random = LegacyRandom(7L)
        repeat(rounds) {
            Fires.tick(world, x, y, z, random)
            if (until()) return true
        }
        return until()
    }

    /** The odds are the jar's own: planks catch and burn, stone does neither. */
    @Test
    fun theOddsComeFromTheRealTables() {
        assertEquals(5, FireOdds.igniteOdds(state("oak_planks")))
        assertEquals(20, FireOdds.burnOdds(state("oak_planks")))
        assertEquals(30, FireOdds.igniteOdds(state("oak_leaves", mapOf("waterlogged" to "false"))))
        assertEquals(0, FireOdds.igniteOdds(Blocks.STONE.stateId))
        assertEquals(0, FireOdds.burnOdds(Blocks.STONE.stateId))
    }

    /** parity: `FireBlock.getIgniteOdds`, where a waterlogged block answers zero whatever it is. */
    @Test
    fun soakedLeavesDoNotCatch() {
        assertEquals(0, FireOdds.igniteOdds(state("oak_leaves", mapOf("waterlogged" to "true"))))
        assertEquals(0, FireOdds.burnOdds(state("oak_leaves", mapOf("waterlogged" to "true"))))
    }

    /** parity: `FireBlock.tick`, which puts fire out when nothing around it will burn. */
    @Test
    fun fireOnStoneGoesOut() {
        world.state.setBlockState(3, 70, 3, Blocks.STONE.stateId)
        world.state.setBlockState(3, 71, 3, state("fire", mapOf("age" to "4")))

        assertTrue(
            burn(3, 71, 3) { world.state.getBlockState(3, 71, 3) == Blocks.AIR.stateId },
            "with nothing to burn it should have died",
        )
    }

    @Test
    fun fireEatsThroughAPileOfPlanks() {
        world.state.setBlockState(6, 70, 3, Blocks.STONE.stateId)
        for (dx in 0..2) world.state.setBlockState(6 + dx, 71, 3, state("oak_planks"))
        world.state.setBlockState(6, 72, 3, state("fire", mapOf("age" to "0")))

        assertTrue(
            burn(6, 72, 3) { world.state.getBlockState(6, 71, 3) != state("oak_planks") },
            "the planks under it should have burned away",
        )
    }

    /** parity: netherrack, which is what `infiniburn` means in practice. */
    @Test
    fun fireOnNetherrackKeepsGoing() {
        world.state.setBlockState(10, 70, 3, state("netherrack"))
        world.state.setBlockState(10, 71, 3, state("fire", mapOf("age" to "15")))

        burn(10, 71, 3, rounds = 200) { false }

        assertTrue(
            Fires.isFire(checkNotNull(world.state.getBlockState(10, 71, 3))),
            "fire on netherrack should still be there",
        )
    }

    @Test
    fun aScheduledFireAsksToBeLookedAtAgain() {
        world.state.setBlockState(13, 70, 3, state("netherrack"))
        world.state.setBlockState(13, 71, 3, state("fire", mapOf("age" to "0")))

        Fires.tick(world, 13, 71, 3, LegacyRandom(1L))

        // Past the longest delay `tickDelay` can pick, so which one it drew does not matter.
        var due = emptyList<IntArray>()
        repeat(45) {
            if (due.isEmpty()) due = world.state.dueBlockTicks()
        }
        assertTrue(due.any { it[0] == 13 && it[1] == 71 && it[2] == 3 }, "it should have scheduled its next turn")
    }
}
