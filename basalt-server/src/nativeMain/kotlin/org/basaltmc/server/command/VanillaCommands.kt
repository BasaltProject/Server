package org.basaltmc.server.command

import org.basaltmc.server.command.commands.DeopCommand
import org.basaltmc.server.command.commands.EmoteCommand
import org.basaltmc.server.command.commands.ExperienceCommand
import org.basaltmc.server.command.commands.GameModeCommand
import org.basaltmc.server.command.commands.HelpCommand
import org.basaltmc.server.command.commands.KickCommand
import org.basaltmc.server.command.commands.KillCommand
import org.basaltmc.server.command.commands.ListPlayersCommand
import org.basaltmc.server.command.commands.OpCommand
import org.basaltmc.server.command.commands.SayCommand
import org.basaltmc.server.command.commands.SeedCommand
import org.basaltmc.server.command.commands.StopCommand
import org.basaltmc.server.command.commands.TeleportCommand

/**
 * Every command Basalt currently understands, in the order they are registered.
 *
 * One class per command under `command/commands/`, the same split vanilla makes under
 * `net.minecraft.server.commands`, and each one reimplements its counterpart's tree shape,
 * permission level and translation keys. The keys matter: the client resolves them itself, so a
 * different key is a visible divergence, not an internal detail.
 *
 * What is *not* here is most of vanilla's command set (the world, scoreboard, data and datapack
 * families) plus the selector filter syntax: both are declared gaps, not silent ones.
 */
public val VANILLA_COMMANDS: List<Command> =
    listOf(
        HelpCommand(),
        ListPlayersCommand(),
        SeedCommand(),
        SayCommand(),
        EmoteCommand(),
        GameModeCommand(),
        TeleportCommand(),
        KillCommand(),
        KickCommand(),
        ExperienceCommand(),
        OpCommand(),
        DeopCommand(),
        StopCommand(),
    )

public fun registerVanillaCommands(dispatcher: CommandDispatcher) {
    for (command in VANILLA_COMMANDS) dispatcher.register(command)
}
