package org.basaltmc.server.command

import org.basaltmc.server.command.commands.DeopCommand
import org.basaltmc.server.command.commands.EmoteCommand
import org.basaltmc.server.command.commands.ExperienceCommand
import org.basaltmc.server.command.commands.GameModeCommand
import org.basaltmc.server.command.commands.GiveCommand
import org.basaltmc.server.command.commands.GroupCommand
import org.basaltmc.server.command.commands.HelpCommand
import org.basaltmc.server.command.commands.ImportPlayersCommand
import org.basaltmc.server.command.commands.KickCommand
import org.basaltmc.server.command.commands.KillCommand
import org.basaltmc.server.command.commands.ListPlayersCommand
import org.basaltmc.server.command.commands.OpCommand
import org.basaltmc.server.command.commands.PlaceholderCommand
import org.basaltmc.server.command.commands.SayCommand
import org.basaltmc.server.command.commands.SeedCommand
import org.basaltmc.server.command.commands.StopCommand
import org.basaltmc.server.command.commands.TeleportCommand
import org.basaltmc.server.command.commands.WorldCommand

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
 *
 * `import-players`, `placeholder` and `world` are the three with no vanilla counterpart, because
 * they administer things vanilla does not have: a player database outside the world directories, a
 * placeholder registry, and more than one world to stand in.
 */
public val VANILLA_COMMANDS: List<Command> =
    listOf(
        HelpCommand(),
        ListPlayersCommand(),
        PlaceholderCommand(),
        SeedCommand(),
        SayCommand(),
        EmoteCommand(),
        GameModeCommand(),
        GiveCommand(),
        TeleportCommand(),
        WorldCommand(),
        KillCommand(),
        KickCommand(),
        ExperienceCommand(),
        OpCommand(),
        DeopCommand(),
        GroupCommand(),
        StopCommand(),
        ImportPlayersCommand(),
    )

public fun registerVanillaCommands(dispatcher: CommandDispatcher) {
    for (command in VANILLA_COMMANDS) dispatcher.register(command)
}
