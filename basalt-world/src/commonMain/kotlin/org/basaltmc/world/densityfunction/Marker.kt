package org.basaltmc.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Marker.Type. Every variant but
 * [BLEND_DENSITY] exists in vanilla purely to mark a subtree for caching (`fillArray`-path
 * optimizations, out of this project's parity contract per §5.4); [Marker] evaluates them all
 * identically. [BLEND_DENSITY] marks where Anvil-format terrain blending would apply (see
 * [BlendAlpha]'s own note on why that does not apply here), which changes its declared bounds to
 * unbounded but not its computed value.
 */
public enum class MarkerType {
    INTERPOLATED,
    FLAT_CACHE,
    CACHE_2D,
    CACHE_ONCE,
    CACHE_ALL_IN_CELL,
    BLEND_DENSITY,
}

/** // parity: net.minecraft.world.level.levelgen.DensityFunctions.Marker. A caching hint that wraps [wrapped] without changing its value. */
public class Marker(
    public val type: MarkerType,
    public val wrapped: DensityFunction,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double = wrapped.compute(context)

    private val isBlendDensity = type == MarkerType.BLEND_DENSITY

    override fun minValue(): Double = if (isBlendDensity) Double.NEGATIVE_INFINITY else wrapped.minValue()

    override fun maxValue(): Double = if (isBlendDensity) Double.POSITIVE_INFINITY else wrapped.maxValue()
}
