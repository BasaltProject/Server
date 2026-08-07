package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom
import kotlin.math.floor

/**
 * A value that may be fixed or randomly rolled when a loot table asks for it (roll counts, item
 * counts, condition chances). Mirrors vanilla's `NumberProvider`; only the two providers real
 * vanilla loot tables use almost everywhere are implemented: `minecraft:constant` and
 * `minecraft:uniform`. Others (`binomial`, `score`, `storage`...) are a documented gap.
 */
public sealed interface NumberProvider {
    public fun getInt(random: BasaltRandom): Int

    public fun getFloat(random: BasaltRandom): Float

    public data class Constant(
        val value: Float,
    ) : NumberProvider {
        override fun getInt(random: BasaltRandom): Int = floor(value).toInt()

        override fun getFloat(random: BasaltRandom): Float = value
    }

    /**
     * `min + random.nextInt(max - min + 1)` for [getInt] (`Mth.nextInt`), `min + random.nextFloat()
     * * (max - min)` for [getFloat] (`Mth.nextFloat`): verified against the real jar's decompiled
     * `Mth` class, not guessed.
     */
    public data class Uniform(
        val min: NumberProvider,
        val max: NumberProvider,
    ) : NumberProvider {
        override fun getInt(random: BasaltRandom): Int {
            val lo = min.getInt(random)
            val hi = max.getInt(random)
            return if (lo >= hi) lo else random.nextIntBetweenInclusive(lo, hi)
        }

        override fun getFloat(random: BasaltRandom): Float {
            val lo = min.getFloat(random)
            val hi = max.getFloat(random)
            return if (lo >= hi) lo else lo + random.nextFloat() * (hi - lo)
        }
    }
}
