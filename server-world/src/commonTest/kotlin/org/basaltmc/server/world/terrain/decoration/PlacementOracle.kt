package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the placement pipeline. What this pins is the order the
 * modifiers draw from the random: vanilla chains them lazily, so a stage-by-stage port would
 * consume it differently and scatter the same feature somewhere else.
 */
class PlacementOracle {
    private fun stockedChunk(): Chunk {
        val chunk = Chunk(1, 2)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70 + ((x * 3 + z * 7) % 5)) {
                    chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }
        return chunk
    }

    private fun run(name: String, modifiers: List<PlacementModifier>) {
        val chunk = stockedChunk()
        val random = WorldgenRandom(LegacyRandom(12345L))
        val placed = StringBuilder()
        placeFrom(chunk, random, DecorationPos(16, 0, 32), modifiers) {
            placed.append("${it.x}/${it.y}/${it.z},")
        }
        // The trailing draw proves the pipeline consumed exactly as many values as vanilla's.
        println("ORACLE $name=$placed after=" + random.nextInt())
    }

    @Test
    fun dump() {
        run("count", listOf(CountPlacement(5), InSquarePlacement, HeightmapPlacement))
        run(
            "nested",
            listOf(
                CountPlacement(IntProvider.uniform(2, 6)),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(3),
                RandomOffsetPlacement(2, 1),
            ),
        )
        run(
            "filters",
            listOf(
                CountPlacement(12),
                InSquarePlacement,
                RarityFilter(2),
                HeightmapPlacement,
                SurfaceWaterDepthFilter(4),
                ConstantOffsetPlacement(1, 2),
            ),
        )
        run(
            "noise",
            listOf(
                NoiseThresholdCountPlacement(-0.8, 5, 10),
                InSquarePlacement,
                UniformHeightRangePlacement(-20, 40),
                CountExtraPlacement(1, 0.25f, 2),
            ),
        )
        run(
            "weighted",
            listOf(
                CountPlacement(IntProvider.Weighted(1 to 3, 4 to 1, 9 to 2)),
                InSquarePlacement,
                NoiseBasedCountPlacement(3, 40.0, 0.5),
                HeightmapPlacement,
            ),
        )
    }
}
