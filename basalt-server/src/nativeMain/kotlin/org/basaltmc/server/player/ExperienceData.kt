package org.basaltmc.server.player

/**
 * The experience bar: a level, a fraction of the way to the next one, and the running total.
 *
 * parity: `Player.giveExperiencePoints`/`giveExperienceLevels`/`getXpNeededForNextLevel`. The
 * level cost curve (7 + 2n below 15, 37 + 5n below 30, 112 + 9n above) and the odd two-step
 * renormalisation in the overflow loop are copied deliberately: the same points always have to land
 * on the same level, and the loop's rounding is part of that.
 */
public class ExperienceData {
    public var level: Int = 0
        private set
    public var progress: Float = 0f
        private set
    public var total: Int = 0
        private set

    public fun xpNeededForNextLevel(): Int =
        when {
            level >= 30 -> 112 + (level - 30) * 9
            level >= 15 -> 37 + (level - 15) * 5
            else -> 7 + level * 2
        }

    public fun giveLevels(amount: Int) {
        level += amount
        if (level < 0) {
            level = 0
            progress = 0f
            total = 0
        }
    }

    public fun givePoints(points: Int) {
        progress += points.toFloat() / xpNeededForNextLevel()
        total = (total.toLong() + points).coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()

        while (progress < 0f) {
            val remaining = progress * xpNeededForNextLevel()
            if (level > 0) {
                giveLevels(-1)
                progress = 1f + remaining / xpNeededForNextLevel()
            } else {
                giveLevels(-1)
                progress = 0f
            }
        }

        while (progress >= 1f) {
            progress = (progress - 1f) * xpNeededForNextLevel()
            giveLevels(1)
            progress /= xpNeededForNextLevel()
        }
    }

    /** parity: `ServerPlayer.setExperienceLevels`, which moves the level and leaves the bar where it is. */
    public fun setLevels(value: Int) {
        level = value
    }

    /**
     * parity: `ServerPlayer.setExperiencePoints`. It keeps the level and moves only the bar, and
     * clamps to `(needed - 1) / needed` so setting exactly one level's worth of points never rolls
     * the bar over into the next level.
     */
    public fun setPoints(value: Int) {
        val needed = xpNeededForNextLevel().toFloat()
        progress = (value / needed).coerceIn(0f, (needed - 1f) / needed)
    }

    /** Puts back exactly what was persisted. Unlike [setLevels] this also restores the running total. */
    public fun restore(
        level: Int,
        progress: Float,
        total: Int,
    ) {
        this.level = level
        this.progress = progress
        this.total = total
    }

    public fun reset() {
        level = 0
        progress = 0f
        total = 0
    }
}
