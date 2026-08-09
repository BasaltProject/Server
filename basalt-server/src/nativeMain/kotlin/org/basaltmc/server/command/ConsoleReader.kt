package org.basaltmc.server.command

import org.basaltmc.io.NativeThread

/**
 * Reads operator commands from stdin on their own thread and runs them through [dispatcher], for
 * as long as the process keeps running.
 *
 * The console runs the same commands players do, at [PermissionLevel.OWNERS], which is how vanilla
 * works too: there is no second, console-only command set. A leading slash is accepted and ignored,
 * because operators type it out of habit and vanilla accepts it as well.
 *
 * This is a separate concern from the EULA prompt this project deliberately avoids at boot
 * (`Main.kt`'s own note, `basalt-assets`' `EulaFile`): that rule is about never blocking startup on
 * interactive input, not about never reading stdin at all. A real vanilla server reads console
 * commands the same way, after it has already started. `readlnOrNull` returns `null` on EOF (piped
 * stdin, redirected from `/dev/null`, a detached process): the loop just stops reading rather than
 * busy-spinning.
 */
public object ConsoleReader {
    public fun start(
        dispatcher: CommandDispatcher,
        source: CommandSource = ConsoleCommandSource(lang = dispatcher.environment.lang),
    ) {
        NativeThread.start {
            while (true) {
                val line = readlnOrNull() ?: break
                val command = line.trim().removePrefix("/")
                if (command.isEmpty()) continue
                dispatcher.execute(source, command)
            }
        }
    }
}
