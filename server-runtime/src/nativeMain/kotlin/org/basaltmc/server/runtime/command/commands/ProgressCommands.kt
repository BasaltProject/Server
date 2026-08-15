package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.SuggestionContext
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.RecipeBook
import org.basaltmc.wire.common.Identifier
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
                            argument("advancement", RegistryArgument.key("minecraft:advancement")) {
                                suggestsFromServer { context -> context.matchingIds(advancements.ids) }
                                executes { context -> selection(context, action, mode, null) }
                                if (mode == "only") {
                                    argument("criterion", WordArgument) {
                                        suggestsFromServer { context -> context.matching(criteriaOf(context)) }
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

    /** Only the criteria of the advancement already named, which is what vanilla completes here. */
    private fun criteriaOf(context: SuggestionContext): Set<String> =
        context.argument<Identifier>("advancement")?.let { advancements[it]?.criteria?.keys }.orEmpty()

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
                    Action.GRANT -> progress.grantCriterion(target.uuid.toString(), id, criterion)
                    Action.REVOKE -> progress.revokeCriterion(target.uuid.toString(), id, criterion)
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
                            if (progress.grantCriterion(target.uuid.toString(), id, criterion)) changed++
                        }
                    Action.REVOKE -> if (progress.revokeAll(target.uuid.toString(), id)) changed++
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
                    argument("recipe", RegistryArgument.key("minecraft:recipe")) {
                        suggestsFromServer { context -> context.matchingIds(recipes.ids) }
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
                    if (verb == "give") {
                        val learned = progress.learn(target.uuid.toString(), id)
                        // parity: `ServerPlayer.awardRecipes` fires the unlock trigger itself,
                        // rather than leaving the advancement to whatever crafting follows.
                        if (learned) {
                            context.environment.advancementTriggers
                                ?.onRecipeUnlocked(target.name, target.uuid.toString(), id)
                        }
                        learned
                    } else {
                        progress.forget(target.uuid.toString(), id)
                    }
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
        if (verb == "take") RecipeBook.remove(target, recipes, ids) else RecipeBook.add(target, recipes, ids)
    }
}
