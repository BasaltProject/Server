package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.wire.text.Component

/**
 * parity: `TriggerCommand`, the one command an ordinary player may run against the scoreboard.
 *
 * It only works on an objective of criterion `trigger` that `/scoreboard players enable` has armed
 * for that player, and running it disarms it again: that is the whole mechanism datapacks use to
 * offer a menu to players who have no permissions at all.
 */
public class TriggerCommand(
    private val scoreboard: Scoreboard,
) : Command(name = "trigger", permissionLevel = PermissionLevel.ALL) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("objective", WordArgument) {
            executes { context -> trigger(context, "simple") { current -> current + 1 } }
            literal("add") {
                argument("value", IntegerArgument()) {
                    executes { context -> trigger(context, "add") { current -> current + context.get<Int>("value") } }
                }
            }
            literal("set") {
                argument("value", IntegerArgument()) {
                    executes { context -> trigger(context, "set") { context.get("value") } }
                }
            }
        }
    }

    private fun trigger(
        context: CommandContext,
        verb: String,
        newValue: (Int) -> Int,
    ): Int {
        val player = context.requirePlayer()
        val name = context.get<String>("objective")
        val objective =
            scoreboard[name]
                ?: throw CommandSyntaxException(Component.translatable("commands.trigger.failed.unprimed"))
        if (!objective.isTrigger) {
            throw CommandSyntaxException(Component.translatable("commands.trigger.failed.invalid"))
        }
        if (!scoreboard.isTriggerEnabled(player.name, objective.name)) {
            throw CommandSyntaxException(Component.translatable("commands.trigger.failed.unprimed"))
        }
        val value = newValue(scoreboard.score(player.name, objective.name) ?: 0)
        scoreboard.setScore(player.name, objective.name, value)
        // Vanilla disarms it here, so a menu cannot be spammed without being re-enabled.
        scoreboard.disableTrigger(player.name, objective.name)
        // parity: the bare form reports only the objective; add and set also report the value.
        context.success(
            if (verb == "simple") {
                Component.translatable("commands.trigger.simple.success", Component.text(objective.name))
            } else {
                Component.translatable(
                    "commands.trigger.$verb.success",
                    Component.text(objective.name),
                    Component.text(value.toString()),
                )
            },
        )
        return value
    }
}
