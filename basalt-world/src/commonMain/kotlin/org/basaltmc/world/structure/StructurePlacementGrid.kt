package org.basaltmc.world.structure

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom

/**
 * // parity: net.minecraft.world.level.levelgen.structure.placement.RandomSpreadStructurePlacement,
 * the `linear` [spreadType] case only (the only one the real `villages.json` uses; `triangular`,
 * used by strongholds and a couple of others, is not ported). Splits the world into a
 * [spacing]-wide grid of chunk cells; each cell has exactly one candidate chunk, chosen
 * pseudorandomly within a [separation]-margined inner square so candidates from adjacent cells
 * never land closer together than [separation] chunks.
 *
 * The per-cell random is a fresh [WorldgenRandom] wrapping a [LegacyRandom] (real
 * `RandomSpreadStructurePlacement.getPotentialStructureChunk` constructs
 * `new WorldgenRandom(new LegacyRandomSource(0L))` every call, not a Xoroshiro source like
 * decoration: confirmed from the decompiled method body), reseeded with the real
 * `setLargeFeatureWithSalt` formula.
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
