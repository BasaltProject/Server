package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.wire.common.Identifier

/**
 * Which entity types are alive, and what they are worth to the world around them.
 *
 * parity: `MobCategory`, straight off each `EntityType`. Everything that is not `misc` is a mob: it
 * has a brain, it counts against a spawn cap, and it despawns by the rules of its own category.
 */
public object Mobs {
    /** parity: `MobCategory.MISC`, which is items, boats, arrows and the rest of the furniture. */
    private const val MISC = "misc"

    public fun isMob(type: Identifier): Boolean = EntityTypeShapes.categoryOf(type) != MISC

    /** parity: `MobCategory.getName`, the name a spawn cap is kept under. */
    public fun categoryOf(type: Identifier): String = EntityTypeShapes.categoryOf(type)

    /** parity: `Attributes.MOVEMENT_SPEED`'s own default for a walking animal. */
    public const val DEFAULT_WALK_SPEED: Double = 0.25
}
