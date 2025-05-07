package org.basaltmc.world

/**
 * Overworld vertical bounds.
 *
 * // parity: matches the 1.18+ Overworld dimension type (`min_y = -64`, `height = 384`), stable
 * across every real Minecraft release since; not yet confirmed against 26.2's actual dimension
 * type registry data, which needs `basalt-datagen` to extract.
 */
public object WorldHeight {
    public const val MIN_Y: Int = -64
    public const val HEIGHT: Int = 384
    public const val MAX_Y: Int = MIN_Y + HEIGHT - 1
    public const val MIN_SECTION: Int = MIN_Y shr 4
    public const val SECTION_COUNT: Int = HEIGHT / 16
}
