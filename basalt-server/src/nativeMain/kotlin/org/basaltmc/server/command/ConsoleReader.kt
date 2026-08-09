package org.basaltmc.server.command

import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis

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
    /**
     * Started once for the life of the process, not once per server.
     *
     * [current] is asked for the dispatcher on every line rather than given one, because `/restart`
     * replaces the whole server inside this process: a reader holding the old dispatcher would keep
     * running commands against a server that has already closed its database. And it cannot simply
     * be started again for the new one, since this thread is parked inside a blocking read of stdin
     * that nothing can interrupt: two readers would take alternate lines from the same terminal.
     *
     * A line that arrives while no server is up **waits** for one rather than being discarded. That
     * window is real at both ends: this thread is started before the first server has finished
     * building, and it stays alive across a restart while the next one is built. Dropping the line
     * there loses a command the operator typed, and piped input makes it certain rather than
     * unlikely: `printf 'stop\n' | basalt-server` delivered `stop` before the server existed and it
     * went nowhere.
     */
    public fun start(current: () -> CommandDispatcher?) {
        NativeThread.start {
            while (true) {
                val line = readlnOrNull() ?: break
                val command = line.trim().removePrefix("/")
                if (command.isEmpty()) continue
                val dispatcher = awaitDispatcher(current)
                if (dispatcher == null) {
                    println("No server is running, so '$command' was not run.")
                    continue
                }
                dispatcher.execute(ConsoleCommandSource(lang = dispatcher.environment.lang), command)
            }
        }
    }

    /** Waits for a server to exist, since one is either starting or being replaced right now. */
    private fun awaitDispatcher(current: () -> CommandDispatcher?): CommandDispatcher? {
        var waited = 0L
        while (waited < MAX_WAIT_MILLIS) {
            current()?.let { return it }
            sleepMillis(POLL_MILLIS)
            waited += POLL_MILLIS
        }
        return current()
    }

    private const val POLL_MILLIS = 25L

    /** Long enough to cover a restart and a cold start; past it, something is wrong and saying so beats hanging. */
    private const val MAX_WAIT_MILLIS = 60_000L
}
