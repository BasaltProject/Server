package org.basaltmc.world.densityfunction

import org.basaltmc.world.GenerationMetrics

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Marker.Type. [FLAT_CACHE] and
 * [CACHE_2D] are honored, against a [DensityCache] carried in [FunctionContext.cache]; the rest
 * evaluate straight through.
 *
 * [FLAT_CACHE] is not merely an optimization: vanilla's `NoiseChunk.FlatCache` quantizes the query
 * to the quart grid and samples at `y = 0`, so honoring it changes computed values, toward vanilla.
 * [CACHE_2D] is exact memoization and changes nothing.
 *
 * [INTERPOLATED], [CACHE_ONCE] and [CACHE_ALL_IN_CELL] stay pass-through: all three are defined
 * against `NoiseChunk`'s own cell-interpolation counters (`CacheOnce` only caches when the context
 * *is* the `NoiseChunk`), and this project interpolates one level up in
 * [org.basaltmc.world.terrain.DensityChunkSampler] rather than per marked subtree. Wiring them up
 * means porting that interpolation scheme first, which is its own change.
 *
 * [BLEND_DENSITY] marks where Anvil-format terrain blending would apply (see [BlendAlpha]'s own
 * note on why that does not apply here), which changes its declared bounds but not its value.
 */
public enum class MarkerType {
    INTERPOLATED,
    FLAT_CACHE,
    CACHE_2D,
    CACHE_ONCE,
    CACHE_ALL_IN_CELL,
    BLEND_DENSITY,
}

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Marker.
 *
 * [slot] indexes this marker's own storage inside whichever [DensityCache] reaches it, taking the
 * place of vanilla's per-chunk tree rewrite. It comes from a plain counter because markers are only
 * ever constructed while a router is being built, which happens once per world before any
 * generation worker exists; nothing constructs one mid-generation. A cache built with fewer slots
 * than the markers it meets is not an error, those markers just evaluate straight through.
 */
public class Marker(
    public val type: MarkerType,
    public val wrapped: DensityFunction,
) : DensityFunction {
    public val slot: Int = nextSlot++

    override fun compute(context: FunctionContext): Double {
        if (GenerationMetrics.enabled) GenerationMetrics.countMarker(type)
        val cache = context.cache ?: return wrapped.compute(context)
        return when (type) {
            MarkerType.FLAT_CACHE -> cache.flatCache(slot, context, wrapped)
            MarkerType.CACHE_2D -> cache.cache2d(slot, context, wrapped)
            else -> wrapped.compute(context)
        }
    }

    private val isBlendDensity = type == MarkerType.BLEND_DENSITY

    override fun minValue(): Double = if (isBlendDensity) Double.NEGATIVE_INFINITY else wrapped.minValue()

    override fun maxValue(): Double = if (isBlendDensity) Double.POSITIVE_INFINITY else wrapped.maxValue()

    public companion object {
        private var nextSlot = 0

        /** How many slots a [DensityCache] needs to cover every marker built so far. */
        public val slotCount: Int get() = nextSlot
    }
}
