package org.basaltmc.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.BlendAlpha. Always `1.0`: real
 * vanilla worlds override this near old-chunk-format boundaries during terrain blending, which
 * this project's world format does not need to replicate (§9 of this project's own roadmap: no
 * Anvil-format terrain to blend against). Kept as a leaf node so density function trees that
 * reference it still type-check and evaluate correctly.
 */
public object BlendAlpha : DensityFunction {
    override fun compute(context: FunctionContext): Double = 1.0

    override fun minValue(): Double = 1.0

    override fun maxValue(): Double = 1.0
}
