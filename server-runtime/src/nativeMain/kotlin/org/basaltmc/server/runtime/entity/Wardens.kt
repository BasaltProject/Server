package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier

/**
 * How angry a warden is at each thing it has noticed.
 *
 * parity: `AngerManagement`: anger is per suspect, climbs to 150 at most, falls by one every tick,
 * and only past eighty does the warden come after you. It is the whole reason a warden ignores
 * somebody who has not made a sound, which is what a target goal reading the follow range does not.
 */
public object Wardens {
    public val TYPE: Identifier = Identifier.vanilla("warden")

    /** parity: `AngerLevel.ANGRY`'s own minimum, and the 150 `increaseAnger` clamps to. */
    public const val ANGRY: Int = 80
    public const val MAX_ANGER: Int = 150

    /** parity: `increaseAngerAt`'s default, what one vibration is worth. */
    public const val HEARD: Int = 35

    /** parity: `hurtServer`, which adds the angry threshold plus twenty on top. */
    public const val HURT: Int = ANGRY + 20

    /** parity: the warden's own `VibrationUser`, which listens sixteen blocks out. */
    public const val HEARING_RANGE: Double = 16.0

    /** parity: `doPush`'s `TOUCH_COOLDOWN`, a second between one shove and the next. */
    public const val TOUCH_COOLDOWN: Int = 20

    public fun isWarden(type: Identifier): Boolean = type == TYPE

    /** parity: `increaseAnger`, which tops out at 150 and answers the new level. */
    public fun anger(
        warden: ServerEntity,
        suspectId: Int,
        amount: Int,
    ): Int {
        val anger = warden.anger ?: mutableMapOf<Int, Int>().also { warden.anger = it }
        val level = minOf((anger[suspectId] ?: 0) + amount, MAX_ANGER)
        anger[suspectId] = level
        return level
    }

    /** parity: `AngerManagement.tick`, one off every suspect every tick and forgotten at one. */
    public fun tick(warden: ServerEntity) {
        val anger = warden.anger ?: return
        if (anger.isEmpty()) return
        val iterator = anger.entries.iterator()
        while (iterator.hasNext()) {
            val entry = iterator.next()
            if (entry.value <= 1) iterator.remove() else entry.setValue(entry.value - 1)
        }
    }

    /** Whoever it is angriest at, once that anger is enough to come after them. */
    public fun target(warden: ServerEntity): Int? {
        val anger = warden.anger ?: return null
        var best: Int? = null
        var bestAnger = ANGRY - 1
        for ((suspect, level) in anger) {
            if (level <= bestAnger) continue
            bestAnger = level
            best = suspect
        }
        return best
    }

    public fun angerAt(
        warden: ServerEntity,
        suspectId: Int,
    ): Int = warden.anger?.get(suspectId) ?: 0
}
