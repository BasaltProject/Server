package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.RecipeBookAddPacket
import org.basaltmc.wire.protocol.play.RecipeBookEntry
import org.basaltmc.wire.protocol.play.RecipeBookRemovePacket
import org.basaltmc.wire.protocol.play.RecipeDisplay
import org.basaltmc.wire.protocol.play.SlotDisplay
import org.basaltmc.wire.text.Component

/**
 * parity: `AdvancementCommands`, all five selection modes.
 *
 * Granting means marking every criterion of the advancement done, which is what vanilla's
 * `AdvancementProgress.grantProgress` ends up doing. Nothing awards one from gameplay yet (the
 * triggers are the gameplay events themselves), so this is currently the only way one completes.
 */
public class AdvancementCommand(
    private val advancements: RealAdvancements,
    private val progress: PlayerProgress,
) : Command(name = "advancement", permissionLevel = PermissionLevel.GAMEMASTERS) {
    private enum class Action { GRANT, REVOKE }

    override fun build(root: CommandNodeBuilder) {
        for (action in Action.entries) {
            root.literal(action.name.lowercase()) {
                argument("targets", PlayersArgument(single = false)) {
                    literal("everything") { executes { context -> everything(context, action) } }
                    for (mode in listOf("only", "from", "through", "until")) {
                        literal(mode) {
                            argument("advancement", ResourceLocationArgument) {
                                executes { context -> selection(context, action, mode, null) }
                                if (mode == "only") {
                                    argument("criterion", WordArgument) {
                                        executes { context ->
                                            selection(context, action, mode, context.get("criterion"))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun everything(
        context: CommandContext,
        action: Action,
    ): Int = apply(context, action, advancements.ids.toList())

    private fun selection(
        context: CommandContext,
        action: Action,
        mode: String,
        criterion: String?,
    ): Int {
        val id = context.get<Identifier>("advancement")
        advancements[id]
            ?: throw CommandSyntaxException(
                Component.translatable("advancement.advancementNotFound", Component.text(id.toString())),
            )
        if (criterion != null) return applyCriterion(context, action, id, criterion)
        val selected =
            when (mode) {
                "only" -> listOf(id)
                "from" -> advancements.descendantsOf(id)
                "until" -> advancements.ancestorsOf(id)
                else -> (advancements.ancestorsOf(id) + advancements.descendantsOf(id)).distinct()
            }
        return apply(context, action, selected)
    }

    private fun applyCriterion(
        context: CommandContext,
        action: Action,
        id: Identifier,
        criterion: String,
    ): Int {
        val advancement = checkNotNull(advancements[id])
        if (criterion !in advancement.criteria) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.advancement.criterionNotFound",
                    Component.text(id.toString()),
                    Component.text(criterion),
                ),
            )
        }
        val targets = context.get<List<PlayerSession>>("targets")
        var changed = 0
        for (target in targets) {
            val moved =
                when (action) {
                    Action.GRANT -> progress.grantCriterion(target.name, id, criterion)
                    Action.REVOKE -> progress.revokeCriterion(target.name, id, criterion)
                }
            if (moved) changed++
        }
        report(context, action, changed, targets, one = true)
        return changed
    }

    private fun apply(
        context: CommandContext,
        action: Action,
        ids: List<Identifier>,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        var changed = 0
        for (target in targets) {
            for (id in ids) {
                val advancement = advancements[id] ?: continue
                when (action) {
                    Action.GRANT ->
                        for (criterion in advancement.criteria.keys) {
                            if (progress.grantCriterion(target.name, id, criterion)) changed++
                        }
                    Action.REVOKE -> if (progress.revokeAll(target.name, id)) changed++
                }
            }
        }
        report(context, action, changed, targets, one = ids.size == 1)
        return changed
    }

    private fun report(
        context: CommandContext,
        action: Action,
        changed: Int,
        targets: List<PlayerSession>,
        one: Boolean,
    ) {
        val verb = if (action == Action.GRANT) "grant" else "revoke"
        if (changed == 0) {
            throw CommandSyntaxException(
                Component.translatable(
                    "commands.advancement.$verb.${if (one) "one" else "many"}.to.one.failure",
                    Component.text(targets.firstOrNull()?.name.orEmpty()),
                ),
            )
        }
        context.success(
            Component.translatable(
                "commands.advancement.$verb.${if (one) "one" else "many"}.to.${
                    if (targets.size == 1) "one" else "many"
                }.success",
                Component.text(changed.toString()),
                Component.text(if (targets.size == 1) targets.single().name else targets.size.toString()),
            ),
            broadcastToAdmins = true,
        )
    }
}

/**
 * parity: `RecipeCommand`.
 *
 * Both halves are real now: the server remembers what a player knows (which is what
 * `limited_crafting` consults before letting them craft), and the client's own recipe book is told,
 * so the recipe appears in it. A recipe whose type this build cannot draw is still remembered; it
 * simply is not sent to the book, because drawing it wrong would be worse than not drawing it.
 */
public class RecipeCommand(
    private val progress: PlayerProgress,
    private val recipes: RealRecipes,
) : Command(name = "recipe", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        for (verb in listOf("give", "take")) {
            root.literal(verb) {
                argument("targets", PlayersArgument(single = false)) {
                    literal("*") { executes { context -> apply(context, verb, null) } }
                    argument("recipe", ResourceLocationArgument) {
                        executes { context -> apply(context, verb, context.get("recipe")) }
                    }
                }
            }
        }
    }

    private fun apply(
        context: CommandContext,
        verb: String,
        recipe: Identifier?,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        val known = recipes.ids
        if (recipe != null && known.isNotEmpty() && recipe !in known) {
            throw CommandSyntaxException(
                Component.translatable("recipe.notFound", Component.text(recipe.toString())),
            )
        }
        val ids = recipe?.let(::listOf) ?: known.toList()
        var changed = 0
        val moved = mutableListOf<Identifier>()
        for (target in targets) {
            for (id in ids) {
                val applied =
                    if (verb == "give") progress.learn(target.name, id) else progress.forget(target.name, id)
                if (applied) {
                    changed++
                    if (id !in moved) moved += id
                }
            }
            if (moved.isNotEmpty()) sendBook(target, verb, moved)
        }
        if (changed == 0) {
            throw CommandSyntaxException(Component.translatable("commands.recipe.$verb.failed"))
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.recipe.$verb.success.single",
                    Component.text(changed.toString()),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.recipe.$verb.success.multiple",
                    Component.text(changed.toString()),
                    Component.text(targets.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return changed
    }

    /** Adds or removes the entries in this player's own book. */
    private fun sendBook(
        target: PlayerSession,
        verb: String,
        ids: List<Identifier>,
    ) {
        val numbered = ids.map { recipes.ids.indexOf(it) }.filter { it >= 0 }
        if (numbered.isEmpty()) return
        if (verb == "take") {
            val packet = RecipeBookRemovePacket(numbered)
            target.connection.sendPlayPacket(RecipeBookRemovePacket.ID) { packet.write(it) }
            return
        }
        val entries = ids.mapNotNull(::entryFor)
        if (entries.isEmpty()) return
        val packet = RecipeBookAddPacket(entries, replace = false)
        target.connection.sendPlayPacket(RecipeBookAddPacket.ID) { packet.write(it) }
    }

    /** One book entry, or null for a recipe this build did not parse and so cannot draw. */
    private fun entryFor(id: Identifier): RecipeBookEntry? {
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
