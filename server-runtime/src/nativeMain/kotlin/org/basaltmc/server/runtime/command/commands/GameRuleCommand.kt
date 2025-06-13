package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.world.GameRule
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.wire.text.Component

/**
 * parity: `GameRuleCommand`. One literal per rule, each optionally taking a value, exactly as
 * vanilla builds the tree, so the client autocompletes rule names.
 *
 * Vanilla also declares every rule a second time under its full `minecraft:` id; that duplicate is
 * skipped, since nothing resolves rules by namespaced id here yet.
 */
public class GameRuleCommand : Command(name = "gamerule", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        for (rule in GameRuleRegistry.ALL) {
            root.literal(rule.id) {
                executes { context -> query(context, rule) }
                when (rule) {
                    is GameRule.Bool ->
                        argument("value", BoolArgument) {
                            executes { context -> set(context, rule, context.get<Boolean>("value").toString()) }
                        }
                    is GameRule.Integer ->
                        argument("value", IntegerArgument(min = rule.min, max = rule.max)) {
                            executes { context -> set(context, rule, context.get<Int>("value").toString()) }
                        }
                }
            }
        }
    }

    private fun query(
        context: CommandContext,
        rule: GameRule<*>,
    ): Int {
        val value = context.world().gameRules.serialized(rule)
        context.success(
            Component.translatable("commands.gamerule.query", Component.text(rule.id), Component.text(value)),
        )
        return commandResult(value)
    }

    private fun set(
        context: CommandContext,
        rule: GameRule<*>,
        value: String,
    ): Int {
        context.world().gameRules.set(rule, value)
        context.success(
            Component.translatable("commands.gamerule.set", Component.text(rule.id), Component.text(value)),
            broadcastToAdmins = true,
        )
        return commandResult(value)
    }

    /** parity: `GameRule.getCommandResult`, which is the integer itself, or 0/1 for a boolean. */
    private fun commandResult(value: String): Int =
        value.toIntOrNull() ?: if (value.toBooleanStrictOrNull() == true) 1 else 0
}
