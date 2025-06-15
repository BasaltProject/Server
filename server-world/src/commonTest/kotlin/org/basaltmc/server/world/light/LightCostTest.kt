package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.wire.common.Identifier
import kotlin.math.sin
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.TimeSource

private val STONE_ID = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("stone")))

/** Rolling terrain around y = 70, the relief a light pass meets in an ordinary overworld chunk. */
private fun terrainChunk(
    chunkX: Int,
    chunkZ: Int,
): Chunk {
    val chunk = Chunk(chunkX, chunkZ, ChunkHeight.OVERWORLD)
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            val worldX = chunkX * 16 + x
            val worldZ = chunkZ * 16 + z
            val height = 70 + (6 * sin(worldX / 20.0) + 6 * sin(worldZ / 17.0)).toInt()
            for (y in ChunkHeight.OVERWORLD.minY..height) chunk.setBlockStateId(x, y, z, STONE_ID)
        }
    }
    return chunk
}

/**
 * What one chunk of light costs, as a guard rather than a benchmark.
 *
 * It runs once per chunk, so it competes with generating one. The first version of this engine took
 * 139 ms a chunk and made exploring crawl; the budget here is what stops that coming back.
 */
class LightCostTest {
    @Test
    fun lightingAChunkStaysWellUnderTheCostOfGeneratingOne() {
        val neighbours = mutableMapOf<Pair<Int, Int>, Chunk>()
        val centre = terrainChunk(0, 0)
        val window = LightWindow(centre) { x, z -> neighbours.getOrPut(x to z) { terrainChunk(x, z) } }
        // Warm the heightmaps, which generation has already paid for by the time light runs.
        window.surfaceRange()

        val runs = 10
        repeat(2) { SkyLight.compute(window) }
        val started = TimeSource.Monotonic.markNow()
        repeat(runs) { SkyLight.compute(window) }
        val perChunk = started.elapsedNow() / runs

        assertTrue(perChunk.inWholeMicroseconds < 10_000, "one chunk took $perChunk")
    }
}
