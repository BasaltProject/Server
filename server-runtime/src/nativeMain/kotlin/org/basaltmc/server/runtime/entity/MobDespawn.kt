package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.BasaltRandom

/**
 * When a mob is forgotten for being too far from anybody.
 *
 * parity: `Mob.checkDespawn`. Two rules, not one: past 128 blocks it goes at once, and between 32
 * and 128 it goes on a one in eight hundred roll, but only after ten minutes of nobody near it.
 * Inside 32 blocks that clock is reset, which is what keeps a farm from emptying while you stand
 * in it.
 */
public object MobDespawn {
    /** parity: the `noActionTime > 600` before the random despawn is even considered. */
    public const val IDLE_BEFORE_RANDOM: Int = 600

    /** parity: `random.nextInt(800)`, the roll a far-away mob makes every tick. */
    public const val RANDOM_ODDS: Int = 800

    /** Whether this mob should be forgotten now. [nearestDistance] is null when nobody is in the world. */
    public fun shouldDespawn(
        mob: ServerEntity,
        category: MobCategory,
        nearestDistance: Double?,
        random: BasaltRandom,
    ): Boolean {
        if (MobPersistence.keeps(mob)) {
            mob.idleTicks = 0
            return false
        }
        // parity: `removeWhenFarAway` answers false for the mobs that live in the misc category, a
        // villager and an iron golem among them, so a village does not empty when you walk away.
        if (category == MobCategory.MISC) return false
        val distance = nearestDistance ?: return false
        val distanceSquared = distance * distance
        val despawn = category.despawnDistance.toDouble() * category.despawnDistance
        if (distanceSquared > despawn) return true
        val keep = MobCategory.NO_DESPAWN_DISTANCE.toDouble() * MobCategory.NO_DESPAWN_DISTANCE
        if (distanceSquared < keep) {
            // parity: standing near it resets the clock, so it is never forgotten under your nose.
            mob.idleTicks = 0
            return false
        }
        return mob.idleTicks > IDLE_BEFORE_RANDOM && random.nextInt(RANDOM_ODDS) == 0
    }
}
