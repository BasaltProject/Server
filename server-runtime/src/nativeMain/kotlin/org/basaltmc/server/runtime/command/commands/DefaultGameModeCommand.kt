package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.GameModeArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.world
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component

/**
 * parity: `DefaultGameModeCommands`, minus its "move everyone who is on the old default" pass,
 * which vanilla only does for players whose mode was never set individually and which needs a flag
 * on the player that does not exist yet.
 */
public class DefaultGameModeCommand : Command(name = "defaultgamemode", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("gamemode", GameModeArgument) {
            executes { context -> set(context, context.get("gamemode")) }
        }
    }

    private fun set(
        context: CommandContext,
        gamemode: Gamemode,
    ): Int {
        context.world().defaultGamemode = gamemode.name.lowercase()
        context.success(
            Component.translatable(
                "commands.defaultgamemode.success",
                Component.translatable("gameMode.${gamemode.name.lowercase()}"),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}
