package org.basaltmc.server.runtime.command

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.wire.common.logging.LogOutput
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.enableTerminalRawMode
import org.basaltmc.wire.io.flushConsole
import org.basaltmc.wire.io.readTerminalByte
import org.basaltmc.wire.io.restoreTerminalMode
import org.basaltmc.wire.io.sleepMillis

/**
 * Reads operator commands from stdin on their own thread and runs them through [dispatcher] for as
 * long as the process lives. Same commands players get, at [PermissionLevel.OWNERS], as in
 * vanilla; a leading slash is accepted and ignored because operators type it anyway.
 *
 * Reading stdin here does not contradict the no-prompt EULA rule: that one is about never blocking
 * startup on input. On EOF the loop stops reading rather than spinning.
 */
public object ConsoleReader {
    /**
     * Started once for the life of the process, not once per server: this thread parks in a
     * blocking stdin read nothing can interrupt, so a second reader would take alternate lines
     * from the same terminal. [current] is therefore asked for the dispatcher per line, or
     * `/restart` would leave it running commands against a closed database.
     *
     * A line arriving while no server is up waits for one. That window is real at both ends, and
     * piped input makes it certain: `printf 'stop\n' | server-runtime` arrives before the server
     * exists.
     */
    public fun start(current: () -> CommandDispatcher?) {
        NativeThread.start {
            // Raw mode is what makes an arrow key reach this program at all; without a terminal it
            // is unavailable and unnecessary, and plain line reading is used instead.
            val editing = enableTerminalRawMode()
            if (editing) {
                // Log lines and the prompt share one terminal. Wipe the prompt before a line is
                // written and draw it again after, or every log line leaves a copy of it behind.
                LogOutput.aroundConsoleLine = { write ->
                    print("\r\u001B[K")
                    write()
                    redrawPrompt?.invoke()
                    flushConsole()
                }
            }
            try {
                readLoop(current, editing)
            } finally {
                LogOutput.aroundConsoleLine = null
                restoreTerminalMode()
            }
        }
    }

    /**
     * How the log hook above puts the prompt back. Set while a line is being edited and cleared
     * between lines, so a log line arriving with nothing half-typed prints on its own.
     */
    private var redrawPrompt: (() -> Unit)? = null

    private fun readLoop(
        current: () -> CommandDispatcher?,
        editing: Boolean,
    ) {
        val history = mutableListOf<String>()
        while (true) {
            val line = (if (editing) readEditedLine(history) else readlnOrNull()) ?: break
            val command = line.trim().removePrefix("/")
            if (command.isEmpty()) continue
            // Kept as typed, and never twice in a row: a history full of repeated `list` is a
            // history you have to scroll past rather than one you can use.
            if (history.lastOrNull() != command) history += command
            if (history.size > MAX_HISTORY) history.removeAt(0)
            val dispatcher = awaitDispatcher(current)
            if (dispatcher == null) {
                println("No server is running, so '$command' was not run.")
                continue
            }
            // A command from the console reaches the world, so it runs on whoever owns it rather
            // than on this thread. With nothing loaded there is nobody to ask, and it runs here.
            val source = ConsoleCommandSource(lang = dispatcher.environment.lang)
            val owner = dispatcher.environment.worlds[dispatcher.environment.defaultWorldName]?.owner
            // The plugin is asked *inside* that, not here: a decider answers where plugins run, and
            // this thread is the one reading stdin. Asking here meant no plugin ever saw a console
            // command, which is exactly what the first live run of this showed.
            val run: () -> Unit = {
                val asked =
                    dispatcher.environment.pluginEvents.emit(PluginEventTypes.SERVER_COMMAND) { event ->
                        event.set(0, "console")
                        event.set(1, command)
                    }
                if (asked?.cancelled == true) {
                    asked.cancelMessage?.let { println(it) }
                } else {
                    dispatcher.execute(source, (asked?.get("command") as? String) ?: command)
                }
            }
            if (owner == null) run() else owner.submit(run)
        }
    }

    /**
     * One line, edited in place: printable characters, backspace, left/right, and up/down through
     * [history]. Returns `null` at end of input, the same as [readlnOrNull].
     *
     * Deliberately small. This is not a readline implementation and does not try to be: no
     * completion, no word motions, no search, no multi-line. It covers what an operator actually
     * reaches for at a server console, which is recalling the last command and fixing a typo in it.
     */
    private fun readEditedLine(history: List<String>): String? {
        val line = StringBuilder()
        var cursor = 0
        // Where up/down currently is in the history. Past the end means "the line being typed",
        // which is what coming back down from the history has to restore.
        var historyIndex = history.size
        var draft = ""

        fun redraw() {
            // Carriage return, the prompt, the line, then clear whatever the old line left behind.
            print("\r> $line\u001B[K")
            if (cursor < line.length) print("\u001B[${line.length - cursor}D")
            flushConsole()
        }

        redrawPrompt = { redraw() }
        redraw()
        while (true) {
            when (val byte = readTerminalByte()) {
                -1 -> {
                    redrawPrompt = null
                    return null
                }
                CARRIAGE_RETURN, NEWLINE -> {
                    redrawPrompt = null
                    print("\r\n")
                    flushConsole()
                    return line.toString()
                }
                BACKSPACE, DELETE -> {
                    if (cursor > 0) {
                        line.deleteAt(cursor - 1)
                        cursor--
                        redraw()
                    }
                }
                ESCAPE -> {
                    // An arrow is escape, then '[', then a letter. Anything else is left alone.
                    if (readTerminalByte() != BRACKET) continue
                    when (readTerminalByte()) {
                        UP -> {
                            if (historyIndex > 0) {
                                if (historyIndex == history.size) draft = line.toString()
                                historyIndex--
                                line.clear().append(history[historyIndex])
                                cursor = line.length
                                redraw()
                            }
                        }
                        DOWN -> {
                            if (historyIndex < history.size) {
                                historyIndex++
                                line.clear()
                                line.append(if (historyIndex == history.size) draft else history[historyIndex])
                                cursor = line.length
                                redraw()
                            }
                        }
                        RIGHT ->
                            if (cursor < line.length) {
                                cursor++
                                redraw()
                            }
                        LEFT ->
                            if (cursor > 0) {
                                cursor--
                                redraw()
                            }
                    }
                }
                else -> {
                    if (byte < FIRST_PRINTABLE) continue
                    line.insert(cursor, byte.toChar())
                    cursor++
                    redraw()
                }
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

    /** Enough to scroll back through a session's worth of commands without growing without bound. */
    private const val MAX_HISTORY = 100

    private const val CARRIAGE_RETURN = 13
    private const val NEWLINE = 10
    private const val BACKSPACE = 8
    private const val DELETE = 127
    private const val ESCAPE = 27
    private const val BRACKET = 91
    private const val UP = 65
    private const val DOWN = 66
    private const val RIGHT = 67
    private const val LEFT = 68

    /** Below this are control characters, which this editor does not act on. */
    private const val FIRST_PRINTABLE = 32

    /** Long enough to cover a restart and a cold start; past it, something is wrong and saying so beats hanging. */
    private const val MAX_WAIT_MILLIS = 60_000L
}
