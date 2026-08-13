package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SetCameraPacket
import org.basaltmc.wire.text.Component

/** parity: `SpectateCommand`, which only works while the spectator is actually in spectator mode. */
public class SpectateCommand : Command(name = "spectate", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> spectate(context, null, context.requirePlayer()) }
        root.argument("target", PlayersArgument(single = true)) {
            executes { context ->
                spectate(context, context.get<List<PlayerSession>>("target").single(), context.requirePlayer())
            }
            argument("player", PlayersArgument(single = true)) {
                executes { context ->
                    spectate(
                        context,
                        context.get<List<PlayerSession>>("target").single(),
                        context.get<List<PlayerSession>>("player").single(),
                    )
                }
            }
        }
    }

    private fun spectate(
        context: CommandContext,
        target: PlayerSession?,
        spectator: PlayerSession,
    ): Int {
        if (target != null && target.uuid == spectator.uuid) {
            throw CommandSyntaxException(Component.translatable("commands.spectate.self"))
        }
        if (spectator.gamemode != Gamemode.SPECTATOR) {
            throw CommandSyntaxException(
                Component.translatable("commands.spectate.not_spectator", Component.text(spectator.name)),
            )
        }
        // Their own entity id puts the camera back, which is how vanilla stops spectating.
        val cameraId = target?.entityId ?: spectator.entityId
        spectator.connection.sendPlayPacket(SetCameraPacket.ID) { SetCameraPacket(cameraId).write(it) }
        context.success(
            if (target != null) {
                Component.translatable("commands.spectate.success.started", Component.text(target.name))
            } else {
                Component.translatable("commands.spectate.success.stopped")
            },
        )
        return 1
    }
}
