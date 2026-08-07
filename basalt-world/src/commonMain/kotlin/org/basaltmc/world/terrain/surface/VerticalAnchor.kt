package org.basaltmc.world.terrain.surface

/** // parity: net.minecraft.world.level.levelgen.WorldGenerationContext. Just the min/height pair `VerticalAnchor.resolveY` needs. */
public class WorldGenerationContext(
    public val minGenY: Int,
    public val genDepth: Int,
)

/** // parity: net.minecraft.world.level.levelgen.VerticalAnchor. A Y coordinate expressed relative to the world's own bounds. */
public sealed interface VerticalAnchor {
    public fun resolveY(context: WorldGenerationContext): Int

    public data class Absolute(
        val y: Int,
    ) : VerticalAnchor {
        override fun resolveY(context: WorldGenerationContext): Int = y
    }

    public data class AboveBottom(
        val offset: Int,
    ) : VerticalAnchor {
        override fun resolveY(context: WorldGenerationContext): Int = context.minGenY + offset
    }

    public data class BelowTop(
        val offset: Int,
    ) : VerticalAnchor {
        override fun resolveY(context: WorldGenerationContext): Int = context.minGenY + context.genDepth - 1 - offset
    }

    public companion object {
        public fun absolute(y: Int): VerticalAnchor = Absolute(y)

        public fun aboveBottom(offset: Int): VerticalAnchor = AboveBottom(offset)

        public fun belowTop(offset: Int): VerticalAnchor = BelowTop(offset)

        public fun bottom(): VerticalAnchor = AboveBottom(0)

        public fun top(): VerticalAnchor = BelowTop(0)
    }
}
