package org.basaltmc.server.command

import org.basaltmc.common.logging.Logger

/** One operator console command, dispatched by [ConsoleCommandRegistry]. */
public fun interface ConsoleCommand {
    public fun execute(args: List<String>)
}

/**
 * Maps command names typed at the console to their [ConsoleCommand] handler. Unknown input logs a
 * message and does nothing else, the same way a real client typing a bad slash command does not
 * crash anything; there is no built-in command here beyond what callers [register].
 */
public class ConsoleCommandRegistry {
    private val commands = mutableMapOf<String, ConsoleCommand>()
    private val logger = Logger("Command")

    public fun register(
        name: String,
        command: ConsoleCommand,
    ) {
        commands[name.lowercase()] = command
    }

    public fun dispatch(line: String) {
        val trimmed = line.trim()
        if (trimmed.isEmpty()) return
        val parts = trimmed.split(Regex("\\s+"))
        val name = parts[0].lowercase()
        val command = commands[name]
        if (command == null) {
            logger.info { "Unknown command '$name'. Type 'help' for a list of commands." }
            return
        }
        command.execute(parts.drop(1))
    }
}
