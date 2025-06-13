package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.TransferPacket
import org.basaltmc.wire.text.Component

/** parity: `TransferCommand`, which hands a player to another server without them reconnecting by hand. */
public class TransferCommand : Command(name = "transfer", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("hostname", WordArgument) {
            executes { context -> transfer(context, DEFAULT_PORT, listOf(context.requirePlayer())) }
            argument("port", IntegerArgument(min = 1, max = 65535)) {
                executes { context -> transfer(context, context.get("port"), listOf(context.requirePlayer())) }
                argument("players", PlayersArgument(single = false)) {
                    executes { context -> transfer(context, context.get("port"), context.get("players")) }
                }
            }
        }
    }

    private fun transfer(
        context: CommandContext,
        port: Int,
        players: List<PlayerSession>,
    ): Int {
        if (players.isEmpty()) {
            throw CommandSyntaxException(Component.translatable("commands.transfer.error.no_players"))
        }
        val host = context.get<String>("hostname")
        val packet = TransferPacket(host, port)
        for (player in players) {
            player.connection.sendPlayPacket(TransferPacket.ID) { packet.write(it) }
        }
        context.success(
            if (players.size == 1) {
                Component.translatable(
                    "commands.transfer.success.single",
                    Component.text(players.single().name),
                    Component.text(host),
                    Component.text(port.toString()),
                )
            } else {
                Component.translatable(
                    "commands.transfer.success.multiple",
                    Component.text(players.size.toString()),
                    Component.text(host),
                    Component.text(port.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return players.size
    }

    private companion object {
        const val DEFAULT_PORT = 25565
    }
}
