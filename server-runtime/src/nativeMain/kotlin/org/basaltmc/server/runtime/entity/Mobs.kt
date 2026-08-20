package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.wire.common.Identifier

/**
 * Which entity types are alive, and what they are worth to the world around them.
 *
 * parity: `MobCategory`, straight off each `EntityType`, plus what that category does not say: a
 * category is where a type spawns, not whether it is alive. A villager and an iron golem are `misc`
 * because nothing spawns them in a wave, and taking that as "not a mob" left both standing still
 * with no AI at all. What settles it is `follow_range`, which only a `Mob` has.
 */
public object Mobs {
    /** parity: `MobCategory.MISC`, which is items, boats, arrows and the rest of the furniture. */
    private const val MISC = "misc"

    public fun isMob(type: Identifier): Boolean =
        EntityTypeShapes.categoryOf(type) != MISC || EntityAttributes.baseValue(type, FOLLOW_RANGE) != null

    /** parity: `Attributes.FOLLOW_RANGE`, which `Mob.createMobAttributes` adds and nothing else has. */
    private const val FOLLOW_RANGE = "follow_range"

    /** parity: `MobCategory.getName`, the name a spawn cap is kept under. */
    public fun categoryOf(type: Identifier): String = EntityTypeShapes.categoryOf(type)

    /** parity: `Attributes.MOVEMENT_SPEED`'s own default for a walking animal. */
    public const val DEFAULT_WALK_SPEED: Double = 0.25
}
