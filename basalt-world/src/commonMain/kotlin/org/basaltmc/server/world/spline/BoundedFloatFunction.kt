package org.basaltmc.server.world.spline

/** // parity: net.minecraft.util.BoundedFloatFunction. A function with declared, self-consistent [minValue]/[maxValue] bounds. */
public interface BoundedFloatFunction<C> {
    public fun apply(c: C): Float

    public fun minValue(): Float

    public fun maxValue(): Float

    public companion object {
        public fun <C> constant(value: Float): BoundedFloatFunction<C> =
            object : BoundedFloatFunction<C> {
                override fun apply(c: C): Float = value

                override fun minValue(): Float = value

                override fun maxValue(): Float = value
            }
    }
}
