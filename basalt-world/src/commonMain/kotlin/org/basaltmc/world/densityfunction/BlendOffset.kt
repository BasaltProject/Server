package org.basaltmc.world.densityfunction

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.BlendOffset. Always `0.0`, see [BlendAlpha]'s own note. */
public object BlendOffset : DensityFunction {
    override fun compute(context: FunctionContext): Double = 0.0

    override fun minValue(): Double = 0.0

    override fun maxValue(): Double = 0.0
}
