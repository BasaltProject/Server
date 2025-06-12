package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.ClearCommand
import org.basaltmc.server.runtime.command.commands.CloneCommand
import org.basaltmc.server.runtime.command.commands.DefaultGameModeCommand
import org.basaltmc.server.runtime.command.commands.DeopCommand
import org.basaltmc.server.runtime.command.commands.DifficultyCommand
import org.basaltmc.server.runtime.command.commands.EmoteCommand
import org.basaltmc.server.runtime.command.commands.ExperienceCommand
import org.basaltmc.server.runtime.command.commands.FillCommand
import org.basaltmc.server.runtime.command.commands.GameModeCommand
import org.basaltmc.server.runtime.command.commands.GameRuleCommand
import org.basaltmc.server.runtime.command.commands.GiveCommand
import org.basaltmc.server.runtime.command.commands.GroupCommand
import org.basaltmc.server.runtime.command.commands.HelpCommand
import org.basaltmc.server.runtime.command.commands.ImportPlayersCommand
import org.basaltmc.server.runtime.command.commands.KickCommand
import org.basaltmc.server.runtime.command.commands.KillCommand
import org.basaltmc.server.runtime.command.commands.ListPlayersCommand
import org.basaltmc.server.runtime.command.commands.MsgCommand
import org.basaltmc.server.runtime.command.commands.OpCommand
import org.basaltmc.server.runtime.command.commands.PlaceholderCommand
import org.basaltmc.server.runtime.command.commands.PlaySoundCommand
import org.basaltmc.server.runtime.command.commands.SayCommand
import org.basaltmc.server.runtime.command.commands.SeedCommand
import org.basaltmc.server.runtime.command.commands.SetBlockCommand
import org.basaltmc.server.runtime.command.commands.SetWorldSpawnCommand
import org.basaltmc.server.runtime.command.commands.StopCommand
import org.basaltmc.server.runtime.command.commands.StopSoundCommand
import org.basaltmc.server.runtime.command.commands.TagCommand
import org.basaltmc.server.runtime.command.commands.TeleportCommand
import org.basaltmc.server.runtime.command.commands.TellRawCommand
import org.basaltmc.server.runtime.command.commands.TimeCommand
import org.basaltmc.server.runtime.command.commands.TitleCommand
import org.basaltmc.server.runtime.command.commands.VersionCommand
import org.basaltmc.server.runtime.command.commands.WeatherCommand
import org.basaltmc.server.runtime.command.commands.WorldCommand

/**
 * Every command Basalt understands, in registration order, one class each under
 * `command/commands/` as vanilla splits them. Each reimplements its counterpart's tree shape,
 * permission level and translation keys: the client resolves the keys, so a different one is
 * visible.
 *
 * Gaps, declared rather than silent: the world, scoreboard, data and datapack families, and the
 * selector filter syntax. `import-players`, `placeholder`, `world` and `reload` have no vanilla
 * counterpart. `reload`, `restart` and `timings` are registered by [org.basaltmc.server.runtime.BasaltServer]
 * instead, each needing something only the running server owns.
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
        SetBlockCommand(),
        GameRuleCommand(),
        TimeCommand(),
        WeatherCommand(),
        DifficultyCommand(),
        DefaultGameModeCommand(),
        SetWorldSpawnCommand(),
        ClearCommand(),
        TellRawCommand(),
        MsgCommand(),
        VersionCommand(),
        TitleCommand(),
        TagCommand(),
        PlaySoundCommand(),
        StopSoundCommand(),
        FillCommand(),
        CloneCommand(),
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
