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
        if (mob.type.path == "wandering_trader") return false
        // Animal.removeWhenFarAway(false). The exceptional natural water animals, zombie horse,
        // camel husk, chicken, cat and ocelot are deliberately absent or handled immediately below.
        if (mob.type.path in NEVER_DESPAWN_ANIMALS) return false
        // parity: `removeWhenFarAway` answers false for the mobs that live in the misc category, a
        // villager and an iron golem among them, so a village does not empty when you walk away.
        if (category == MobCategory.MISC) return false
        // Chicken alone opts back into normal distance despawning while carrying a jockey.
        if (mob.type.path == "chicken" && mob.sourceNbt?.getBoolean("IsChickenJockey") != true) return false
        // Cat.removeWhenFarAway protects every newly spawned stray for its first two minutes.
        if (mob.type.path == "cat" && mob.tickCount <= CAT_DESPAWN_GRACE_TICKS) return false
        // Ocelot has the same grace, and trusting one changes the answer permanently to false.
        if (
            mob.type.path == "ocelot" &&
            (mob.tickCount <= CAT_DESPAWN_GRACE_TICKS || mob.sourceNbt?.getBoolean("Trusting") == true)
        ) {
            return false
        }
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
        // parity: PatrollingMonster.removeWhenFarAway. A patrol may leave the player but does not
        // take the ordinary random-despawn roll until it crosses the hard 128-block boundary.
        if (mob.patrolling && distanceSquared <= PATROL_HARD_DESPAWN_SQUARED) return false
        return mob.idleTicks > IDLE_BEFORE_RANDOM && random.nextInt(RANDOM_ODDS) == 0
    }

    private const val PATROL_HARD_DESPAWN_SQUARED = 16_384.0
    private const val CAT_DESPAWN_GRACE_TICKS = 2_400

    private val NEVER_DESPAWN_ANIMALS =
        setOf(
            "armadillo",
            "bee",
            "camel",
            "cow",
            "donkey",
            "fox",
            "frog",
            "goat",
            "happy_ghast",
            "horse",
            "llama",
            "mooshroom",
            "mule",
            "panda",
            "parrot",
            "pig",
            "polar_bear",
            "rabbit",
            "sheep",
            "skeleton_horse",
            "sniffer",
            "strider",
            "trader_llama",
            "turtle",
            "wolf",
        )
}
