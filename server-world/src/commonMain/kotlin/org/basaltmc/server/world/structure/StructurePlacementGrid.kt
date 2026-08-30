package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom

/**
 * // parity: RandomSpreadStructurePlacement. One candidate chunk per [spacing]-wide cell, drawn
 * inside a [separation]-margined square so neighbouring candidates stay that far apart.
 *
 * The per-cell random is a fresh [WorldgenRandom] over [LegacyRandom], not the xoroshiro source
 * decoration uses, reseeded with `setLargeFeatureWithSalt`.
 */
public class StructurePlacementGrid(
    public val spacing: Int,
    public val separation: Int,
    public val salt: Int,
    public val spreadType: SpreadType = SpreadType.LINEAR,
    /** Below 1, the share of candidate chunks that actually get the structure. */
    public val frequency: Float = 1.0f,
    public val frequencyReduction: FrequencyReduction = FrequencyReduction.DEFAULT,
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
        val spreadX = spreadType.evaluate(random, limit)
        val spreadZ = spreadType.evaluate(random, limit)
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

    /**
     * Whether ([chunkX], [chunkZ]) really gets the structure: its own cell's candidate, and then
     * the frequency roll. // parity: StructurePlacement.isStructureChunk, minus the exclusion zone,
     * which no structure wired here declares.
     */
    public fun isStructureChunk(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Boolean =
        isPlacementChunk(seed, chunkX, chunkZ) &&
            frequencyReduction.shouldGenerate(seed, salt, chunkX, chunkZ, frequency)

    /**
     * // parity: StructurePlacement.FrequencyReductionMethod. Four ways of rolling the same
     * probability, kept apart because each seeds its random differently and vanilla still names
     * every one of them in a datapack.
     */
    public enum class FrequencyReduction {
        DEFAULT,
        LEGACY_TYPE_1,
        LEGACY_TYPE_2,
        LEGACY_TYPE_3,
        ;

        internal fun shouldGenerate(
            seed: Long,
            salt: Int,
            chunkX: Int,
            chunkZ: Int,
            frequency: Float,
        ): Boolean {
            if (frequency >= 1.0f) return true
            val random = WorldgenRandom(LegacyRandom(0L))
            return when (this) {
                DEFAULT -> {
                    random.setLargeFeatureWithSalt(seed, chunkX, chunkZ, salt)
                    random.nextFloat() < frequency
                }
                // The pillager outpost roll, which reads the chunk coordinates as block ones.
                LEGACY_TYPE_1 -> {
                    random.setSeed((chunkX shr 4).toLong() xor ((chunkZ shr 4).toLong() shl 4) xor seed)
                    random.nextInt()
                    random.nextInt((1.0f / frequency).toInt()) == 0
                }
                LEGACY_TYPE_2 -> {
                    random.setLargeFeatureWithSalt(seed, chunkX, chunkZ, LEGACY_TYPE_2_SALT)
                    random.nextFloat() < frequency
                }
                LEGACY_TYPE_3 -> {
                    random.setLargeFeatureSeed(seed, chunkX, chunkZ)
                    random.nextDouble() < frequency
                }
            }
        }

        private companion object {
            const val LEGACY_TYPE_2_SALT = 10387320
        }
    }

    public enum class SpreadType {
        LINEAR,
        TRIANGULAR,
        ;

        internal fun evaluate(
            random: WorldgenRandom,
            limit: Int,
        ): Int =
            when (this) {
                LINEAR -> random.nextInt(limit)
                TRIANGULAR -> (random.nextInt(limit) + random.nextInt(limit)) / 2
            }
    }
}
