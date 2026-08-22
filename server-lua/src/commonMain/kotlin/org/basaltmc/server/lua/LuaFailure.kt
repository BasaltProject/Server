package org.basaltmc.server.lua

import net.blueva.luak.LuaError

/**
 * What a plugin did wrong, said the way its author can act on it.
 *
 * Lua puts the chunk name and line in the message itself (`arena/main.lua:12: ...`), so the work
 * here is keeping that intact and naming the plugin rather than wrapping it in a host stack trace
 * nobody can use.
 */
public class LuaFailure(
    public val plugin: String,
    public val error: LuaError,
) : Exception(error.message ?: "error in $plugin") {
    /** The message with the plugin in front, which is the line that goes to the console. */
    public val report: String get() = "[$plugin] ${error.message ?: "error"}"

    /** Whether the plugin ran out of the instructions it was given for this turn, or was stopped. */
    public val outOfBudget: Boolean
        get() = error.message?.let { it.contains(BUDGET_MESSAGE) || it.contains(INTERRUPTED_MESSAGE) } == true

    /** Whether the plugin asked for more memory than it is allowed. */
    public val outOfMemory: Boolean get() = error.message?.contains(MEMORY_MESSAGE) == true

    public companion object {
        internal const val BUDGET_MESSAGE = "instruction budget exhausted"
        internal const val INTERRUPTED_MESSAGE = "interrupted"
        internal const val MEMORY_MESSAGE = "not enough memory"
    }
}
