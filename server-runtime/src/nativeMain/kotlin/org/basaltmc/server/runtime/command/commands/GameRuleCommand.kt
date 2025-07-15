package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.GameRule
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage

/**
 * parity: `GameRuleCommand`. One literal per rule, each optionally taking a value, exactly as
 * vanilla builds the tree, so the client autocompletes rule names.
 *
 * Every rule is declared twice, bare and under its `minecraft:` id, which is what vanilla does and
 * what makes `/gamerule minecraft:keep_inventory` work.
 */
public class GameRuleCommand : Command(name = "gamerule", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        for (rule in GameRuleRegistry.ALL) {
            for (name in listOf(rule.id, "minecraft:${rule.id}")) {
                root.literal(name) {
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
        val world = context.world()
        val change =
            context.environment.pluginEvents.emit(PluginEventTypes.WORLD_GAME_RULE_CHANGE) { event ->
                event.set(0, world.name)
                event.set(1, rule.id)
                event.set(2, value)
                event.set(
                    3,
                    when (rule) {
                        is GameRule.Bool -> world.gameRules[rule].toString()
                        is GameRule.Integer -> world.gameRules[rule].toString()
                    },
                )
                event.set(4, context.source.name)
            }
        if (change?.cancelled == true) {
            change.cancelMessage?.let { context.source.sendSystemMessage(MiniMessage.parse(it)) }
            return 0
        }
        world.gameRules.set(rule, value)
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
