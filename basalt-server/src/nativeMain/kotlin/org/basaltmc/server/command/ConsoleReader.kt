package org.basaltmc.server.command

import org.basaltmc.io.NativeThread

/**
 * Reads operator commands from stdin on their own thread and dispatches them to [registry], for
 * as long as the process keeps running. This is a separate concern from the EULA prompt this
 * project deliberately avoids at boot (`Main.kt`'s own note, `basalt-assets`' `EulaFile`): that
 * rule is about never blocking startup on interactive input, not about never reading stdin at
 * all. A real vanilla server reads console commands the same way, after it has already started.
 * `readlnOrNull` returns `null` on EOF (piped-in stdin, redirected from `/dev/null`, a detached
 * process): the loop just stops reading rather than busy-spinning.
 */
public object ConsoleReader {
    public fun start(registry: ConsoleCommandRegistry) {
        NativeThread.start {
            while (true) {
                val line = readlnOrNull() ?: break
                registry.dispatch(line)
            }
        }
    }
}
