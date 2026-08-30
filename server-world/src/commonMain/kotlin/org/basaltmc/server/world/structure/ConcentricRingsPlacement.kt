package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/**
 * // parity: ConcentricRingsStructurePlacement plus `generateRingPositions`. The stronghold is the
 * only structure not spread over a grid: a fixed list of [count] chunks on rings around the origin.
 * The whole list is built at once because each position moves the shared random on.
 */
public class ConcentricRingsPlacement(
    private val distance: Int,
    private val spread: Int,
    private val count: Int,
    private val preferredBiomes: Set<String>,
    /** The biome at a quart position, which is what the search samples. */
    private val noiseBiomeAt: (quartX: Int, quartY: Int, quartZ: Int) -> String,
) {
    /** Chunk coordinates, in the order the rings were laid out. */
    public fun ringPositions(seed: Long): List<Pair<Int, Int>> {
        if (count == 0) return emptyList()
        val random = LegacyRandom(0L)
        random.setSeed(seed)
        val positions = ArrayList<Pair<Int, Int>>(count)
        var angle = random.nextDouble() * PI * 2.0
        var positionInCircle = 0
        var circle = 0
        var ringSpread = spread
        for (i in 0 until count) {
            val ringDistance = 4.0 * distance + distance * circle * 6.0 + (random.nextDouble() - 0.5) * (distance * 2.5)
            // parity: `Math.round` is floor(v + 0.5), which for a negative half lands one higher
            // than Kotlin's round-half-away-from-zero.
            val initialX = floor(cos(angle) * ringDistance + 0.5).toInt()
            val initialZ = floor(sin(angle) * ringDistance + 0.5).toInt()
            // parity: the fork is drawn here, in the main loop, even though the search that uses it
            // runs on another thread; that is what keeps the list reproducible.
            val searchRandom = random.fork()
            positions += search(initialX, initialZ, searchRandom) ?: (initialX to initialZ)
            angle += PI * 2.0 / ringSpread
            if (++positionInCircle == ringSpread) {
                circle++
                positionInCircle = 0
                ringSpread += 2 * ringSpread / (circle + 1)
                ringSpread = minOf(ringSpread, count - i)
                angle += random.nextDouble() * PI * 2.0
            }
        }
        return positions
    }

    /**
     * // parity: BiomeSource.findBiomeHorizontal with `findClosest` false, which is not a spiral out
     * from the centre: it sweeps the whole square at the outermost radius once and keeps a uniform
     * pick among every cell that matched, by reservoir sampling.
     */
    private fun search(
        chunkX: Int,
        chunkZ: Int,
        random: org.basaltmc.server.random.BasaltRandom,
    ): Pair<Int, Int>? {
        val originX = (chunkX shl 4) + 8
        val originZ = (chunkZ shl 4) + 8
        val centreQuartX = originX shr 2
        val centreQuartZ = originZ shr 2
        val radius = SEARCH_RADIUS shr 2
        var resultX = 0
        var resultZ = 0
        var found = 0
        for (dz in -radius..radius) {
            for (dx in -radius..radius) {
                val quartX = centreQuartX + dx
                val quartZ = centreQuartZ + dz
                if (noiseBiomeAt(quartX, 0, quartZ) !in preferredBiomes) continue
                if (found == 0 || random.nextInt(found + 1) == 0) {
                    resultX = quartX shl 2
                    resultZ = quartZ shl 2
                }
                found++
            }
        }
        if (found == 0) return null
        return (resultX shr 4) to (resultZ shr 4)
    }

    private companion object {
        const val SEARCH_RADIUS = 112
    }
}
