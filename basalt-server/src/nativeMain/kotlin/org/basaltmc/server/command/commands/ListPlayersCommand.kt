package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component

/** parity: `ListPlayersCommand`. Available to everyone, as it is in vanilla. */
public class ListPlayersCommand : Command(name = "list") {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> list(context) { Component.text(it.name) } }
        root.literal("uuids") {
            executes { context ->
                list(context) { player ->
                    Component.translatable(
                        "commands.list.nameAndId",
                        Component.text(player.name),
                        Component.text(player.uuid.toString()),
                    )
                }
            }
        }
    }

    private fun list(
        context: CommandContext,
        format: (PlayerSession) -> Component,
    ): Int {
        val players = context.environment.players.snapshot()
        context.success(
            Component.translatable(
                "commands.list.players",
                Component.text(players.size.toString()),
                Component.text(context.environment.maxPlayers.toString()),
                Component.join(Component.text(", "), players.map(format)),
            ),
        )
        return players.size
    }
}
