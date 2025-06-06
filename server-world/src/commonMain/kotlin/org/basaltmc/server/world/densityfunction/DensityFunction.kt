package org.basaltmc.server.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunction.FunctionContext (the single-point
 * variant). [cache] is what vanilla gets by rewriting the tree per chunk instead: see
 * [DensityCache]. Null means "no chunk scope", and every [Marker] then evaluates straight through.
 */
public class FunctionContext(
    public var blockX: Int,
    public var blockY: Int,
    public var blockZ: Int,
    public val cache: DensityCache? = null,
) {
    /**
     * Moves this context to another block, so a caller sampling a whole chunk reuses one instead of
     * allocating one per block. Safe only because no node keeps a context past its own [compute]:
     * they read the three coordinates and pass the same instance down.
     */
    internal fun moveTo(
        x: Int,
        y: Int,
        z: Int,
    ) {
        blockX = x
        blockY = y
        blockZ = z
    }
}

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunction. The expression tree vanilla's
 * noise router builds terrain shape from. Only [compute] (single-point evaluation) and the
 * declared bounds ([minValue]/[maxValue], which several node types need to fold constants and
 * short-circuit `min`/`max`) are modeled: `fillArray` (a bulk-evaluation fast path, a performance
 * optimization only, out of this project's parity contract per its own roadmap §5.4),
 * `mapChildren`/`mapAll` (tree-rewriting visitors, needed only once trees are built from a
 * data-driven codec, which does not exist here yet) and `codec` (that same data-driven loading)
 * are not implemented.
 */
public interface DensityFunction {
    public fun compute(context: FunctionContext): Double

    public fun minValue(): Double

    public fun maxValue(): Double
}
