package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.RecipeBookAddPacket
import org.basaltmc.wire.protocol.play.RecipeBookEntry
import org.basaltmc.wire.protocol.play.RecipeBookRemovePacket
import org.basaltmc.wire.protocol.play.RecipeDisplay
import org.basaltmc.wire.protocol.play.SlotDisplay

/**
 * One player's recipe book, as the client draws it.
 *
 * Shared because two things unlock a recipe: `/recipe give`, and an advancement whose reward is a
 * recipe, which is how vanilla unlocks most of the book. Both have to put the same entry in it.
 */
public object RecipeBook {
    /** Puts [ids] in [target]'s book, skipping any this build did not parse and so cannot draw. */
    public fun add(
        target: PlayerSession,
        recipes: RealRecipes,
        ids: List<Identifier>,
    ) {
        val entries = ids.mapNotNull { entryFor(recipes, it) }
        if (entries.isEmpty()) return
        val packet = RecipeBookAddPacket(entries, replace = false)
        target.connection.sendPlayPacket(RecipeBookAddPacket.ID) { packet.write(it) }
    }

    public fun remove(
        target: PlayerSession,
        recipes: RealRecipes,
        ids: List<Identifier>,
    ) {
        val numbered = ids.map { recipes.ids.indexOf(it) }.filter { it >= 0 }
        if (numbered.isEmpty()) return
        val packet = RecipeBookRemovePacket(numbered)
        target.connection.sendPlayPacket(RecipeBookRemovePacket.ID) { packet.write(it) }
    }

    /** One book entry, or null for a recipe this build did not parse and so cannot draw. */
    private fun entryFor(
        recipes: RealRecipes,
        id: Identifier,
    ): RecipeBookEntry? {
        val recipe = recipes.recipeFor(id) ?: return null
        val recipeNumber = recipes.ids.indexOf(id)
        if (recipeNumber < 0) return null
        val result = slotFor(setOf(recipe.result.item)) ?: return null
        val station = slotFor(setOf(Identifier.vanilla("crafting_table"))) ?: SlotDisplay.Empty
        val display =
            when (recipe) {
                is Recipe.Shaped ->
                    RecipeDisplay.Shaped(
                        width = recipe.width,
                        height = recipe.height,
                        ingredients = recipe.ingredients.map { it?.let { i -> slotFor(i.items) } ?: SlotDisplay.Empty },
                        result = result,
                        craftingStation = station,
                    )
                is Recipe.Shapeless ->
                    RecipeDisplay.Shapeless(
                        ingredients = recipe.ingredients.map { slotFor(it.items) ?: SlotDisplay.Empty },
                        result = result,
                        craftingStation = station,
                    )
            }
        val requirements =
            when (recipe) {
                is Recipe.Shaped -> recipe.ingredients.filterNotNull().map { itemIds(it.items) }
                is Recipe.Shapeless -> recipe.ingredients.map { itemIds(it.items) }
            }.filter { it.isNotEmpty() }
        return RecipeBookEntry(
            recipeId = recipeNumber,
            display = display,
            category = RecipeBookEntry.CRAFTING_MISC_CATEGORY,
            craftingRequirements = requirements,
        )
    }

    /** One slot: a single item, or a choice between the items an ingredient accepts. */
    private fun slotFor(items: Set<Identifier>): SlotDisplay? {
        val ids = itemIds(items)
        return when {
            ids.isEmpty() -> null
            ids.size == 1 -> SlotDisplay.Item(ids.single())
            else -> SlotDisplay.Composite(ids.map { SlotDisplay.Item(it) })
        }
    }

    /** Protocol ids, dropping anything this build's item registry does not know. */
    private fun itemIds(items: Set<Identifier>): List<Int> =
        items.mapNotNull { Items.REGISTRY.byIdentifier(it)?.protocolId }
}
