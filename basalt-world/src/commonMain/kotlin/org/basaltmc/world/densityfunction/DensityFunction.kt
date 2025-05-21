package org.basaltmc.world.densityfunction

/** // parity: net.minecraft.world.level.levelgen.DensityFunction.FunctionContext (the single-point variant). */
public class FunctionContext(
    public val blockX: Int,
    public val blockY: Int,
    public val blockZ: Int,
)

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
