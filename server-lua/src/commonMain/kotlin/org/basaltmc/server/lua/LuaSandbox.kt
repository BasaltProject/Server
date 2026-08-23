package org.basaltmc.server.lua

import net.blueva.luak.Budget
import net.blueva.luak.Globals
import net.blueva.luak.LuaError
import net.blueva.luak.LuaFunction
import net.blueva.luak.LuaValue
import net.blueva.luak.Prototype
import net.blueva.luak.Varargs
import net.blueva.luak.io.ByteArrayInputStream
import net.blueva.luak.io.InputStream
import net.blueva.luak.lib.LuaPlatform
import net.blueva.luak.lib.ResourceFinder
import net.blueva.luak.lib.VarArgFunction

/** What one plugin may spend before the server takes its turn back. */
public data class LuaLimits(
    /**
     * Instructions per handover, not per lifetime: every entry from the server refills it, and
     * what the plugin does inside spends from the same ceiling, coroutines included.
     *
     * The default is measured rather than guessed: this interpreter runs about 18,500 instructions
     * a millisecond on the machine this was written on, so a quarter of a million is around 13ms,
     * a quarter of a tick. Plenty for anything a handler legitimately does in one turn, and a hard
     * stop well inside the tick for anything that has run away.
     */
    val instructionsPerTurn: Long = 250_000,
    val memoryBytes: Long = 32L * 1024 * 1024,
)

/**
 * One Lua state with everything a plugin must not reach taken out of it.
 *
 * There is one of these per plugin and lane (see the plugin host), never one shared: a state is
 * owned like everything else, and two lanes running at once would corrupt a shared one.
 */
public class LuaSandbox(
    /** What errors and log lines call this state. */
    public val name: String,
    public val limits: LuaLimits = LuaLimits(),
    seed: Long = 0L,
    /** Where `require` looks. Returning null is "no such module", not an error. */
    private val sources: (String) -> ByteArray? = { null },
    /** Where `print` goes. Nothing a plugin runs writes to stdout on its own. */
    private val printLine: (String) -> Unit = {},
) {
    /** Attached to the state, so the interpreter counts against it without the `debug` library. */
    public val budget: Budget =
        Budget().also {
            it.instructions = limits.instructionsPerTurn
        }

    public val globals: Globals = LuaPlatform.standardGlobals()

    init {
        globals.budget = budget
        globals.memoryceiling = limits.memoryBytes
        globals.startmemorycount()
        globals.seedrandom(seed)
        globals.finder =
            object : ResourceFinder {
                override fun findResource(filename: String?): InputStream? {
                    val bytes = filename?.let(sources) ?: return null
                    return ByteArrayInputStream(bytes)
                }
            }
        strip()
    }

    /**
     * Compiles once, for every lane to share.
     *
     * A [Prototype] is finished code and constants and is never written to again, so unlike a
     * loaded environment it can be handed to as many states as there are lanes.
     */
    public fun compile(
        source: String,
        chunkName: String,
    ): Prototype = globals.compile(source, chunkName)

    /** Makes [prototype] callable in this state. */
    public fun bind(prototype: Prototype): LuaFunction = globals.bind(prototype)

    /** Gives the state its ceiling back. Every entry from the server is a fresh turn. */
    public fun refill() {
        budget.refill()
    }

    /** Stops whatever is running, from a thread that is not the one running it. */
    public fun interrupt() {
        budget.interrupt()
    }

    /** Starts the memory tally again, at a point where the lane is quiet. */
    public fun resetMemoryCount() {
        globals.startmemorycount()
    }

    public val memoryUsed: Long get() = globals.memorycharged

    /**
     * Takes out what a plugin has no business reaching.
     *
     * `io.popen`, `os.execute` and `package.loadlib` are not implemented on Kotlin/Native at all,
     * so the list below is the rest: whole-file access, the process environment, and the two ways
     * of loading code from somewhere other than the package.
     */
    private fun strip() {
        for (name in REMOVED_GLOBALS) globals.set(name, Lua.NIL)
        val os = globals.get("os") ?: Lua.NIL
        if (os.istable()) for (name in REMOVED_OS) os.set(name, Lua.NIL)
        // Taking the global away is not enough: `require("io")` hands back the same library from
        // `package.loaded`, which is a door left open behind the one that was locked.
        val loaded = (globals.get("package") ?: Lua.NIL).takeIf { it.istable() }?.get("loaded")
        if (loaded != null && loaded.istable()) {
            // `package` itself is in there too, so requiring it by name would hand back the map of
            // doors a moment after the global was taken away.
            for (name in REMOVED_GLOBALS + "package") loaded.set(name, Lua.NIL)
        }
        // And `package` itself goes, since it is the map of every other door: `loaded`, `preload`
        // and the searchers that would put one back. `require` keeps working, because what it looks
        // through is the finder this class installed, not this table.
        globals.set("package", Lua.NIL)
        globals.set("print", PrintTo(printLine))
        globals.set("load", TextOnlyLoad(globals.get("load") ?: Lua.NIL))
    }

    /** Runs [body] with Lua errors turned into [LuaFailure], which carries the plugin's own line. */
    public inline fun <T> guard(body: () -> T): T =
        try {
            body()
        } catch (e: LuaError) {
            throw LuaFailure(name, e)
        }

    private companion object {
        /**
         * `debug` is not loaded to begin with; it is named here so that adding it back is a
         * deliberate act rather than a side effect of switching to another way of building the
         * globals.
         */
        val REMOVED_GLOBALS = listOf("io", "debug", "dofile", "loadfile", "collectgarbage")
        val REMOVED_OS = listOf("execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale")
    }
}

/** `print`, going to the plugin's own log instead of the server's stdout. */
private class PrintTo(
    private val sink: (String) -> Unit,
) : VarArgFunction() {
    override fun invoke(args: Varargs): Varargs {
        val line = StringBuilder()
        for (i in 1..args.narg()) {
            if (i > 1) line.append('\t')
            line.append(args.arg(i).tojstring())
        }
        sink(line.toString())
        return Lua.NONE
    }
}

/**
 * `load`, with the mode forced to text.
 *
 * A binary chunk is unverified bytecode: a malformed one is a way to crash the interpreter from
 * inside the sandbox, which is the one thing a sandbox cannot allow.
 */
private class TextOnlyLoad(
    private val original: LuaValue,
) : VarArgFunction() {
    override fun invoke(args: Varargs): Varargs =
        if (args.narg() >= 4) {
            original.invoke(LuaValue.varargsOf(arrayOf(args.arg(1), args.arg(2), TEXT, args.arg(4))))
        } else {
            original.invoke(LuaValue.varargsOf(arrayOf(args.arg(1), args.arg(2), TEXT)))
        }

    private companion object {
        val TEXT: LuaValue = LuaValue.valueOf("t")
    }
}
