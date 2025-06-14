package org.basaltmc.server.world.advancement

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * What a criterion is waiting for, as far as this server can tell.
 *
 * Typed rather than raw JSON, and deliberately incomplete: a trigger with no case here never fires
 * ([Unsupported]), which is the honest answer while the gameplay event behind it does not exist.
 * The two cases that do exist cover 3299 of vanilla's 3500-odd criteria.
 */
public sealed interface CriterionCondition {
    /** `minecraft:inventory_changed`: every matcher must be satisfied by something in the inventory. */
    public data class InventoryItems(
        val matchers: List<ItemMatcher>,
    ) : CriterionCondition

    /** `minecraft:recipe_unlocked`: fires when that recipe enters the player's book. */
    public data class RecipeUnlocked(
        val recipe: Identifier,
    ) : CriterionCondition

    /** A trigger this build has no event for. Never fires, and says so rather than firing blindly. */
    public data class Unsupported(
        val trigger: Identifier,
    ) : CriterionCondition
}

/** One `items` entry of an item predicate: a set of ids, or a tag standing for one. */
public data class ItemMatcher(
    val items: List<Identifier> = emptyList(),
    val tag: Identifier? = null,
)

/** One named criterion within an advancement: which trigger grants it, and under what condition. */
public data class AdvancementCriterion(
    val trigger: Identifier,
    val condition: CriterionCondition = CriterionCondition.Unsupported(trigger),
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
 * What a completed advancement says in chat. parity: `DisplayInfo` plus `AdvancementType`, of which
 * only the parts an announcement needs are kept: nothing here draws the advancement screen.
 */
public data class AdvancementDisplay(
    val title: Component,
    val description: Component,
    /** `task`, `goal` or `challenge`, which decides the message key and the colour. */
    val type: String = "task",
    val announceToChat: Boolean = true,
)

/**
 * One advancement definition. Mirrors vanilla's `Advancement` record: [criteria] name the trigger a
 * player must satisfy and the condition it must satisfy it under, and [requirements] combine them
 * into a single completion test.
 */
public data class Advancement(
    val parent: Identifier?,
    val criteria: Map<String, AdvancementCriterion>,
    val requirements: AdvancementRequirements,
    val rewards: AdvancementRewards = AdvancementRewards(),
    val display: AdvancementDisplay? = null,
) {
    public fun isComplete(granted: Set<String>): Boolean = requirements.isComplete(granted)
}
