package org.basaltmc.world.terrain.surface

import org.basaltmc.common.Identifier
import org.basaltmc.random.BasaltRandom
import org.basaltmc.registry.Blocks
import kotlin.math.floor

/** // parity: `SurfaceSystem.generateBands`, the per-world 192-block badlands clay palette. */
public class BadlandsClayBands(
    random: BasaltRandom,
) {
    private val bands: IntArray = generateBands(random)

    public fun stateId(
        y: Int,
        offsetNoise: Double,
    ): Int {
        val offset = floor(offsetNoise * 4.0 + 0.5).toInt()
        return bands[(y + offset + bands.size) % bands.size]
    }

    internal fun copyStateIds(): IntArray = bands.copyOf()

    private fun generateBands(random: BasaltRandom): IntArray {
        val result = IntArray(BAND_COUNT) { TERRACOTTA }
        var index = 0
        while (index < result.size) {
            index += random.nextInt(5) + 1
            if (index < result.size) result[index] = ORANGE_TERRACOTTA
            index++
        }
        makeBands(random, result, 1, YELLOW_TERRACOTTA)
        makeBands(random, result, 2, BROWN_TERRACOTTA)
        makeBands(random, result, 1, RED_TERRACOTTA)

        val whiteBandCount = random.nextIntBetweenInclusive(9, 15)
        var placed = 0
        var start = 0
        while (placed < whiteBandCount && start < result.size) {
            result[start] = WHITE_TERRACOTTA
            if (start - 1 > 0 && random.nextBoolean()) result[start - 1] = LIGHT_GRAY_TERRACOTTA
            if (start + 1 < result.size && random.nextBoolean()) result[start + 1] = LIGHT_GRAY_TERRACOTTA
            placed++
            start += random.nextInt(16) + 4
        }
        return result
    }

    private fun makeBands(
        random: BasaltRandom,
        bands: IntArray,
        baseWidth: Int,
        stateId: Int,
    ) {
        repeat(random.nextIntBetweenInclusive(6, 15)) {
            val width = baseWidth + random.nextInt(3)
            val start = random.nextInt(bands.size)
            var offset = 0
            while (start + offset < bands.size && offset < width) {
                bands[start + offset] = stateId
                offset++
            }
        }
    }

    private companion object {
        const val BAND_COUNT = 192

        fun state(name: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block minecraft:$name")

        val TERRACOTTA = state("terracotta")
        val ORANGE_TERRACOTTA = state("orange_terracotta")
        val YELLOW_TERRACOTTA = state("yellow_terracotta")
        val BROWN_TERRACOTTA = state("brown_terracotta")
        val RED_TERRACOTTA = state("red_terracotta")
        val WHITE_TERRACOTTA = state("white_terracotta")
        val LIGHT_GRAY_TERRACOTTA = state("light_gray_terracotta")
    }
}
