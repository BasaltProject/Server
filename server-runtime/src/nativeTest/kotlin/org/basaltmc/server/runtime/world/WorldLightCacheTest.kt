package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/**
 * The light a chunk works out is kept even when it was worked out without every neighbour.
 *
 * It used to be thrown away in that case, so every query on a chunk at the edge of the loaded area
 * relit the whole chunk: over two milliseconds a time, thousands of times a tick, which is where
 * the tick rate went. Correctness comes from dropping it when the missing neighbour arrives.
 */
class WorldLightCacheTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration("light-cache", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/light-cache",
            logger = Logger("WorldLightCacheTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    @Test
    fun anEdgeChunkKeepsTheLightItWorkedOut() {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        assertNull(chunk.cachedSkyLight(), "nobody has asked yet")

        WorldLight.rawBrightness(world, 8, 70, 8)

        assertNotNull(chunk.cachedSkyLight(), "asked once with no neighbours loaded, and kept")
    }

    @Test
    fun aNeighbourArrivingThrowsThatLightAway() {
        val chunk = world.state.getOrGenerateChunk(0, 0)
        WorldLight.rawBrightness(world, 8, 70, 8)
        assertNotNull(chunk.cachedSkyLight())

        world.state.getOrGenerateChunk(1, 0)

        assertNull(chunk.cachedSkyLight(), "the next query has to see the chunk that just arrived")
    }
}
