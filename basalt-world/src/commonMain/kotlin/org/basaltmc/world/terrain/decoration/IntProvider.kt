package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom

/** // parity: net.minecraft.util.valueproviders.IntProvider, just the two variants real tree configs actually use. */
public sealed interface IntProvider {
    public fun sample(random: WorldgenRandom): Int

    public data class Constant(
        val value: Int,
    ) : IntProvider {
        override fun sample(random: WorldgenRandom): Int = value
    }

    /** // parity: net.minecraft.util.valueproviders.UniformInt. Inclusive on both ends. Always draws, even when `min == max` (`nextInt(1)`), matching the real `Mth.randomBetweenInclusive`. */
    public data class Uniform(
        val min: Int,
        val max: Int,
    ) : IntProvider {
        override fun sample(random: WorldgenRandom): Int = min + random.nextInt(max - min + 1)
    }

    /**
     * // parity: net.minecraft.util.valueproviders.WeightedListInt over `ConstantInt` entries only
     * (the only shape real tree configs use): [entries] are (value, weight) pairs in real insertion
     * order, so a cumulative walk over them is identical to the real Flat selector's own
     * repeat-by-weight layout.
     */
    public class Weighted(
        private vararg val entries: Pair<Int, Int>,
    ) : IntProvider {
        private val totalWeight = entries.sumOf { it.second }

        override fun sample(random: WorldgenRandom): Int {
            var roll = random.nextInt(totalWeight)
            for ((value, weight) in entries) {
                if (roll < weight) return value
                roll -= weight
            }
            return entries.last().first
        }
    }

    public companion object {
        public fun of(value: Int): IntProvider = Constant(value)

        public fun uniform(
            min: Int,
            max: Int,
        ): IntProvider = Uniform(min, max)
    }
}
