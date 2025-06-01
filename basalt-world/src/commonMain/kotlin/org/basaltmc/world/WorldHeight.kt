package org.basaltmc.world

/**
 * Overworld vertical-bound compatibility constants. New dimension-aware code uses [ChunkHeight].
 *
 * // parity: net.minecraft.world.level.levelgen.NoiseSettings.OVERWORLD_NOISE_SETTINGS
 * (`create(-64, 384, 1, 2)`), confirmed directly against the real 26.2 jar, not just assumed
 * stable from older versions.
 */
public object WorldHeight {
    public const val MIN_Y: Int = -64
    public const val HEIGHT: Int = 384
    public const val MAX_Y: Int = MIN_Y + HEIGHT - 1
    public const val MIN_SECTION: Int = MIN_Y shr 4
    public const val SECTION_COUNT: Int = HEIGHT / 16
}
