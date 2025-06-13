package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.MessageArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.wire.text.Component

/** parity: `TeamMsgCommand`, whose `chat.type.team.text` key is what draws the team-coloured brackets. */
public class TeamMsgCommand(
    private val teams: Teams,
) : Command(
        name = "teammsg",
        permissionLevel = PermissionLevel.ALL,
        aliases = listOf("tm"),
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("message", MessageArgument) {
            executes { context ->
                val player = context.requirePlayer()
                val team =
                    teams.teamOf(player.name)
                        ?: throw CommandSyntaxException(Component.translatable("commands.teammsg.failed.noteam"))
                val body = Component.text(context.get<String>("message"))
                val message =
                    Component.translatable(
                        "chat.type.team.text",
                        team.displayName,
                        Component.text(player.name),
                        body,
                    )
                val online = context.environment.players.snapshot()
                for (member in online.filter { it.name in team.members }) member.sendSystemMessage(message)
                context.environment.logger.info {
                    "[Team ${team.name}] <${player.name}> ${context.get<String>(
                        "message",
                    )}"
                }
                team.members.size
            }
        }
    }
}
