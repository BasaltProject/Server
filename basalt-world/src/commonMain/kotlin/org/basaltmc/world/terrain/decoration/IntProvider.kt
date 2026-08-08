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

    /** // parity: net.minecraft.util.valueproviders.UniformInt. Inclusive on both ends. */
    public data class Uniform(
        val min: Int,
        val max: Int,
    ) : IntProvider {
        override fun sample(random: WorldgenRandom): Int = min + random.nextInt(max - min + 1)
    }

    public companion object {
        public fun of(value: Int): IntProvider = Constant(value)

        public fun uniform(
            min: Int,
            max: Int,
        ): IntProvider = Uniform(min, max)
    }
}
