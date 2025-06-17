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

class RandomTicksTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration(
                    "ticks",
                    WorldDimension.OVERWORLD,
                    WorldType.VOID,
                    seed = 0L,
                    isDefault = true,
                ),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/randomticks",
            logger = Logger("RandomTicksTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    private val ticks = RandomTicks()

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

    private fun ageAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        world.state.getBlockState(x, y, z)?.let {
            BlockStateDefinitions.propertiesOf(it)["age"]?.toIntOrNull()
        }

    /**
     * Ticks one position until [until] holds or the budget runs out.
     *
     * The position is given rather than waited for: at the real rate, the picker lands on any one
     * block about three times in four thousand ticks, so a test that waited would be measuring the
     * odds and not the behaviour. Those odds are [thePickerStaysInsideItsChunk]'s business.
     */
    private fun run(
        x: Int,
        y: Int,
        z: Int,
        rounds: Int = 2000,
        until: () -> Boolean,
    ): Boolean {
        val random = LegacyRandom(1L)
        repeat(rounds) {
            ticks.tickBlock(world, x, y, z, random)
            if (until()) return true
        }
        return until()
    }

    @Test
    fun wheatOnWetFarmlandGrowsToFull() {
        world.state.setBlockState(4, 64, 4, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(4, 65, 4, state("wheat", mapOf("age" to "0")))

        assertTrue(
            run(4, 65, 4) { ageAt(4, 65, 4) == WHEAT_MAX_AGE },
            "wheat should reach age 7, got ${ageAt(4, 65, 4)}",
        )
    }

    /**
     * parity: `CropBlock.randomTick` asks for light nine or better.
     *
     * Buried in a shell rather than merely roofed: sky light goes round a lid and comes back up
     * underneath it, exactly as it does in the game, so a one-block roof leaves a crop at twelve.
     */
    @Test
    fun wheatInTheDarkStaysPut() {
        for (dx in -1..1) {
            for (dy in -1..1) {
                for (dz in -1..1) world.state.setBlockState(6 + dx, 66 + dy, 6 + dz, Blocks.STONE.stateId)
            }
        }
        // The shell stays whole: swapping its floor for farmland would let the light in from below.
        world.state.setBlockState(6, 66, 6, state("wheat", mapOf("age" to "0")))

        assertEquals(0, WorldLight.rawBrightness(world, 6, 66, 6), "the shell should let no light in")
        run(6, 66, 6, rounds = 200) { false }
        assertEquals(0, ageAt(6, 66, 6), "a crop in the dark should not have grown")
    }

    /**
     * parity: `SugarCaneBlock.randomTick`. Each block of the stalk grows on its own tick, so this
     * ticks the top one in turn the way the picker would rather than the bottom one forever.
     */
    @Test
    fun sugarCaneClimbsToThreeAndStops() {
        world.state.setBlockState(8, 64, 8, state("sand"))
        world.state.setBlockState(8, 65, 8, state("sugar_cane", mapOf("age" to "0")))

        for (y in 65..68) run(8, y, 8, rounds = 200) { false }

        val cane = BlockStateDefinitions.blockOf(checkNotNull(world.state.getBlockState(8, 67, 8)))
        assertEquals(Identifier.vanilla("sugar_cane"), cane, "cane should have climbed to three blocks")
        assertEquals(Blocks.AIR.stateId, world.state.getBlockState(8, 68, 8), "and no further")
    }

    /** parity: `NetherWartBlock.randomTick`, one roll in ten and no light involved. */
    @Test
    fun netherWartGrowsWithoutLight() {
        world.state.setBlockState(10, 64, 10, state("soul_sand"))
        world.state.setBlockState(10, 65, 10, state("nether_wart", mapOf("age" to "0")))
        world.state.setBlockState(10, 66, 10, Blocks.STONE.stateId)

        assertTrue(run(10, 65, 10) { ageAt(10, 65, 10) == NETHER_WART_MAX_AGE }, "wart should reach age 3")
    }

    @Test
    fun nothingGrowsWhenTheGameRuleIsZero() {
        world.state.setBlockState(12, 64, 12, state("farmland", mapOf("moisture" to "7")))
        world.state.setBlockState(12, 65, 12, state("wheat", mapOf("age" to "0")))

        repeat(500) { ticks.tick(world, listOf(intArrayOf(0, 0)), speed = 0, random = LegacyRandom(1L)) }

        assertEquals(0, ageAt(12, 65, 12))
    }

    /**
     * parity: `Level.getBlockRandomPos`, which is what decides where a tick lands. Its walk covers
     * every block of a section and never leaves it, and a crop growing at all depends on that.
     */
    @Test
    fun thePickerStaysInsideItsChunk() {
        world.state.setBlockState(1, 70, 1, state("wheat", mapOf("age" to "0")))
        val random = LegacyRandom(1L)
        var grew = false
        repeat(20_000) {
            if (ticks.tick(world, listOf(intArrayOf(0, 0)), speed = 3, random = random).isNotEmpty()) grew = true
        }
        assertTrue(grew, "over enough ticks the picker should have landed on the one crop in the chunk")
    }

    private companion object {
        const val WHEAT_MAX_AGE = 7
        const val NETHER_WART_MAX_AGE = 3
    }
}
