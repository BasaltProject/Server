package org.basaltmc.world.advancement

import org.basaltmc.registry.Identifier

/** One named criterion within an advancement: which trigger grants it. Its condition JSON is kept unparsed. */
public data class AdvancementCriterion(
    val trigger: Identifier,
)

/**
 * AND-of-OR criterion groups. Mirrors `AdvancementRequirements.test`: every group must have at least
 * one of its criteria granted, and an advancement with no groups is never complete.
 */
public data class AdvancementRequirements(
    val groups: List<List<String>>,
) {
    public fun isComplete(granted: Set<String>): Boolean {
        if (groups.isEmpty()) return false
        return groups.all { group -> group.any { it in granted } }
    }
}

public data class AdvancementRewards(
    val experience: Int = 0,
    val loot: List<Identifier> = emptyList(),
    val recipes: List<Identifier> = emptyList(),
)

/**
 * One advancement definition. Mirrors vanilla's `Advancement` record: [criteria] name the trigger a
 * player must satisfy (`minecraft:inventory_changed`, `minecraft:tick`...), and [requirements]
 * combine them into a single completion test. Granting a criterion from a live gameplay event (the
 * trigger's own condition check, e.g. "did this inventory change include an emerald") isn't
 * implemented here: that needs the event system built alongside gameplay parity, so [isComplete]
 * only combines criteria that are already known to be granted, it never observes gameplay itself.
 * `display` isn't modeled at all, since nothing consumes it yet (no advancement toast/UI exists).
 */
public data class Advancement(
    val parent: Identifier?,
    val criteria: Map<String, AdvancementCriterion>,
    val requirements: AdvancementRequirements,
    val rewards: AdvancementRewards = AdvancementRewards(),
) {
    public fun isComplete(granted: Set<String>): Boolean = requirements.isComplete(granted)
}
