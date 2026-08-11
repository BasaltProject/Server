package org.basaltmc.world.structure

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom

/**
 * // parity: RandomSpreadStructurePlacement, the `linear` [spreadType] only, which is what
 * `villages.json` uses; `triangular` is not ported. One candidate chunk per [spacing]-wide cell,
 * drawn inside a [separation]-margined square so neighbouring candidates stay that far apart.
 *
 * The per-cell random is a fresh [WorldgenRandom] over [LegacyRandom], not the xoroshiro source
 * decoration uses, reseeded with `setLargeFeatureWithSalt`.
 */
public class StructurePlacementGrid(
    public val spacing: Int,
    public val separation: Int,
    public val salt: Int,
) {
    init {
        require(spacing > separation) { "spacing must be larger than separation" }
    }

    /** The one candidate chunk (in chunk coordinates) for the grid cell containing ([sourceX], [sourceZ]). */
    public fun candidateChunk(
        seed: Long,
        sourceX: Int,
        sourceZ: Int,
    ): Pair<Int, Int> {
        val cellX = sourceX.floorDiv(spacing)
        val cellZ = sourceZ.floorDiv(spacing)
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureWithSalt(seed, cellX, cellZ, salt)
        val limit = spacing - separation
        val spreadX = random.nextInt(limit)
        val spreadZ = random.nextInt(limit)
        return (cellX * spacing + spreadX) to (cellZ * spacing + spreadZ)
    }

    /** Whether ([chunkX], [chunkZ]) is itself the candidate chunk for its own grid cell. */
    public fun isPlacementChunk(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean {
        val (candidateX, candidateZ) = candidateChunk(seed, chunkX, chunkZ)
        return candidateX == chunkX && candidateZ == chunkZ
    }
}
