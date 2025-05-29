package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.CommandSyntaxException
import org.basaltmc.server.command.IntegerArgument
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.PlayersArgument
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component

/** parity: `ExperienceCommand`, `/xp` alias included. Points is the default when neither literal is given. */
public class ExperienceCommand :
    Command(
        name = "experience",
        permissionLevel = PermissionLevel.GAMEMASTERS,
        aliases = listOf("xp"),
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("add") {
            argument("target", PlayersArgument(single = false)) {
                argument("amount", IntegerArgument()) {
                    executes { context -> add(context, points = true) }
                    literal("points") { executes { context -> add(context, points = true) } }
                    literal("levels") { executes { context -> add(context, points = false) } }
                }
            }
        }
        root.literal("set") {
            argument("target", PlayersArgument(single = false)) {
                argument("amount", IntegerArgument(min = 0)) {
                    executes { context -> set(context, points = true) }
                    literal("points") { executes { context -> set(context, points = true) } }
                    literal("levels") { executes { context -> set(context, points = false) } }
                }
            }
        }
        root.literal("query") {
            argument("target", PlayersArgument(single = true)) {
                literal("points") { executes { context -> query(context, points = true) } }
                literal("levels") { executes { context -> query(context, points = false) } }
            }
        }
    }

    private fun add(
        context: CommandContext,
        points: Boolean,
    ): Int {
        val targets = context.get<List<PlayerSession>>("target")
        val amount = context.get<Int>("amount")
        for (target in targets) {
            if (points) target.experience.givePoints(amount) else target.experience.giveLevels(amount)
            target.connection.syncExperience()
        }
        context.success(message("add", points, amount, targets), broadcastToAdmins = true)
        return targets.size
    }

    private fun set(
        context: CommandContext,
        points: Boolean,
    ): Int {
        val targets = context.get<List<PlayerSession>>("target")
        val amount = context.get<Int>("amount")
        var applied = 0
        for (target in targets) {
            if (points) {
                // parity: setting more points than the current level is worth is refused outright
                // rather than silently rolling over into the next level.
                if (amount >= target.experience.xpNeededForNextLevel()) continue
                target.experience.setPoints(amount)
            } else {
                target.experience.setLevels(amount)
            }
            target.connection.syncExperience()
            applied++
        }
        if (applied == 0) throw CommandSyntaxException(Component.translatable("commands.experience.set.points.invalid"))
        context.success(message("set", points, amount, targets), broadcastToAdmins = true)
        return targets.size
    }

    private fun query(
        context: CommandContext,
        points: Boolean,
    ): Int {
        val target = context.get<List<PlayerSession>>("target").single()
        val result =
            if (points) {
                (target.experience.progress * target.experience.xpNeededForNextLevel()).toInt()
            } else {
                target.experience.level
            }
        context.success(
            Component.translatable(
                "commands.experience.query.${if (points) "points" else "levels"}",
                Component.text(target.name),
                Component.text(result.toString()),
            ),
        )
        return result
    }

    private fun message(
        action: String,
        points: Boolean,
        amount: Int,
        targets: List<PlayerSession>,
    ): Component {
        val type = if (points) "points" else "levels"
        return if (targets.size == 1) {
            Component.translatable(
                "commands.experience.$action.$type.success.single",
                Component.text(amount.toString()),
                Component.text(targets.single().name),
            )
        } else {
            Component.translatable(
                "commands.experience.$action.$type.success.multiple",
                Component.text(amount.toString()),
                Component.text(targets.size.toString()),
            )
        }
    }
}
