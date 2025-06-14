package org.basaltmc.server.runtime.world

import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.world.advancement.Advancement
import org.basaltmc.server.world.advancement.CriterionCondition
import org.basaltmc.server.world.advancement.ItemMatcher
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/** One criterion of one advancement, as the trigger index refers to it. */
private data class CriterionRef(
    val advancement: Identifier,
    val criterion: String,
    val condition: CriterionCondition,
)

/**
 * What a player just did, and which criteria that satisfies.
 *
 * parity: `CriteriaTriggers` plus `PlayerAdvancements.award`, indexed once at startup so a
 * picked-up item costs a map lookup rather than a walk over three thousand criteria.
 *
 * The limit is which triggers exist, not which conditions: `inventory_changed` and
 * `recipe_unlocked` are what this build can observe, and the rest arrive with the systems that
 * produce their events.
 */
public class AdvancementTriggers(
    private val advancements: RealAdvancements,
    private val progress: PlayerProgress,
    /** Which items a tag holds, for the matchers written as `#minecraft:planks`. */
    private val itemTagMembers: (Identifier) -> Set<Identifier> = { emptySet() },
    /** Announces a completed advancement, when the game rule and the advancement both allow it. */
    private val announce: (player: String, message: Component) -> Unit = { _, _ -> },
    /** Grants the experience an advancement rewards. */
    private val awardExperience: (player: String, amount: Int) -> Unit = { _, _ -> },
    /** Teaches a recipe an advancement rewards, which is how vanilla unlocks most of the book. */
    private val awardRecipe: (player: String, recipe: Identifier) -> Unit = { _, _ -> },
) {
    /** Every `inventory_changed` criterion, indexed by an item that can satisfy one of its matchers. */
    private val byItem: Map<Identifier, List<CriterionRef>> = buildInventoryIndex()

    /** Every `recipe_unlocked` criterion, by the recipe it waits for. */
    private val byRecipe: Map<Identifier, List<CriterionRef>> = buildRecipeIndex()

    /**
     * parity: `InventoryChangeTrigger`, which vanilla fires on any inventory change rather than on
     * the item that changed, so an advancement whose matcher was already satisfied still completes
     * the first time anything else moves.
     */
    public fun onInventoryChanged(
        player: String,
        uuid: String,
        inventory: List<StoredItemStack?>,
    ) {
        val held = inventory.filterNotNull()
        if (held.isEmpty()) return
        val items = held.map { Identifier.parse(it.id) }.toSet()
        val candidates = items.flatMap { byItem[it].orEmpty() }.distinct()
        for (candidate in candidates) {
            val condition = candidate.condition as? CriterionCondition.InventoryItems ?: continue
            if (!condition.matchers.all { matcher -> items.any { matches(matcher, it) } }) continue
            grant(player, uuid, candidate)
        }
    }

    /** parity: `RecipeUnlockedTrigger`, fired when a recipe enters the book rather than when it is used. */
    public fun onRecipeUnlocked(
        player: String,
        uuid: String,
        recipe: Identifier,
    ) {
        for (candidate in byRecipe[recipe].orEmpty()) grant(player, uuid, candidate)
    }

    private fun grant(
        player: String,
        uuid: String,
        candidate: CriterionRef,
    ) {
        val advancement = advancements[candidate.advancement] ?: return
        val wasDone = advancement.isComplete(progress.grantedCriteria(uuid, candidate.advancement))
        if (!progress.grantCriterion(uuid, candidate.advancement, candidate.criterion)) return
        if (wasDone) return
        if (!advancement.isComplete(progress.grantedCriteria(uuid, candidate.advancement))) return
        complete(player, uuid, candidate.advancement, advancement)
    }

    private fun complete(
        player: String,
        uuid: String,
        id: Identifier,
        advancement: Advancement,
    ) {
        if (advancement.rewards.experience > 0) awardExperience(player, advancement.rewards.experience)
        for (recipe in advancement.rewards.recipes) {
            if (progress.learn(uuid, recipe)) awardRecipe(player, recipe)
        }
        val display = advancement.display ?: return
        if (!display.announceToChat) return
        announce(
            player,
            Component.translatable(
                "chat.type.advancement.${display.type}",
                Component.text(player),
                name(display.title, display.type),
            ),
        )
    }

    /** parity: `Advancement.name`, the title in square brackets and the type's own colour. */
    private fun name(
        title: Component,
        type: String,
    ): Component {
        val colour = if (type == "challenge") TextColor.LIGHT_PURPLE else TextColor.GREEN
        return Component
            .text("[")
            .append(title)
            .append(Component.text("]"))
            .color(colour)
    }

    private fun matches(
        matcher: ItemMatcher,
        item: Identifier,
    ): Boolean {
        if (item in matcher.items) return true
        return matcher.tag?.let { item in itemTagMembers(it) } == true
    }

    private fun buildInventoryIndex(): Map<Identifier, List<CriterionRef>> {
        val index = mutableMapOf<Identifier, MutableList<CriterionRef>>()
        for (id in advancements.ids) {
            val advancement = advancements[id] ?: continue
            for ((name, criterion) in advancement.criteria) {
                val condition = criterion.condition as? CriterionCondition.InventoryItems ?: continue
                val reference = CriterionRef(id, name, condition)
                // Indexed by every item that can satisfy the *first* matcher: the rest are checked
                // when the candidate comes up, and one entry per matcher would repeat the work.
                val first = condition.matchers.firstOrNull() ?: continue
                val items = first.items + (first.tag?.let(itemTagMembers) ?: emptySet())
                for (item in items) index.getOrPut(item) { mutableListOf() } += reference
            }
        }
        return index
    }

    private fun buildRecipeIndex(): Map<Identifier, List<CriterionRef>> {
        val index = mutableMapOf<Identifier, MutableList<CriterionRef>>()
        for (id in advancements.ids) {
            val advancement = advancements[id] ?: continue
            for ((name, criterion) in advancement.criteria) {
                val condition = criterion.condition as? CriterionCondition.RecipeUnlocked ?: continue
                index.getOrPut(condition.recipe) { mutableListOf() } += CriterionRef(id, name, condition)
            }
        }
        return index
    }
}
