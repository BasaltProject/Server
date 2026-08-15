package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private fun ruleWorld(dimension: WorldDimension): LoadedWorld =
    LoadedWorld(
        declaration = WorldDeclaration(dimension.configName, dimension, WorldType.VOID, seed = 0L, isDefault = false),
        settings = WorldSettings.VANILLA,
        metadata = WorldMetadata(0L, 0, 64, 0),
        worldsRoot = "build/test-data/rules-${dimension.configName}",
        logger = Logger("RulesTest").apply { minLevel = LogLevel.ERROR },
        generationWorkers = 1,
    )

/** What each dimension does on its own every tick: weather, and whether time runs. */
class DimensionRulesTest {
    private val worlds = WorldDimension.entries.associateWith(::ruleWorld)

    @AfterTest
    fun stopGenerating() {
        for (world in worlds.values) world.stopGenerating(drainTimeoutMillis = 100)
    }

    /** parity: `Level.canHaveWeather`: skylight, no ceiling, and not the End. */
    @Test
    fun onlyTheOverworldHasWeather() {
        assertTrue(WorldDimension.OVERWORLD.canHaveWeather)
        assertFalse(WorldDimension.NETHER.canHaveWeather)
        assertFalse(WorldDimension.END.canHaveWeather)
    }

    @Test
    fun rainNeverStartsWhereThereIsNoWeather() {
        val nether = checkNotNull(worlds[WorldDimension.NETHER])
        nether.weather.rainTime = 1

        repeat(5) { nether.tickTime() }

        assertFalse(nether.weather.raining)
        assertEquals(1, nether.weather.rainTime)
    }

    /** parity: `dimension_type.default_clock`, which the Nether has not got. */
    @Test
    fun timeRunsEverywhereButTheNether() {
        val overworld = checkNotNull(worlds[WorldDimension.OVERWORLD])
        val nether = checkNotNull(worlds[WorldDimension.NETHER])
        val end = checkNotNull(worlds[WorldDimension.END])
        val before = Triple(overworld.clock.totalTicks, nether.clock.totalTicks, end.clock.totalTicks)

        repeat(10) {
            overworld.tickTime()
            nether.tickTime()
            end.tickTime()
        }

        assertEquals(before.first + 10, overworld.clock.totalTicks)
        assertEquals(before.second, nether.clock.totalTicks)
        assertEquals(before.third + 10, end.clock.totalTicks)
    }

    /** The tick count is not the clock: it runs in every world, which is what `/tick` reports. */
    @Test
    fun theTickCounterStillRunsInTheNether() {
        val nether = checkNotNull(worlds[WorldDimension.NETHER])
        val before = nether.gameTime

        repeat(3) { nether.tickTime() }

        assertEquals(before + 3, nether.gameTime)
    }

    /** parity: `DimensionType.coordinateScale`, which is the whole of the 1:8. */
    @Test
    fun theNetherIsEightTimesSmaller() {
        assertEquals(1.0, WorldDimension.OVERWORLD.coordinateScale)
        assertEquals(8.0, WorldDimension.NETHER.coordinateScale)
        assertEquals(1.0, WorldDimension.END.coordinateScale)
    }
}
