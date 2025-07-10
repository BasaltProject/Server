package org.basaltmc.server.lua

import net.blueva.luak.LuaError
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import net.blueva.luak.lib.VarArgFunction
import kotlin.coroutines.Continuation
import kotlin.coroutines.EmptyCoroutineContext
import kotlin.coroutines.resume
import kotlin.coroutines.startCoroutine
import kotlin.coroutines.suspendCoroutine

/**
 * A call into Lua that is allowed to pause.
 *
 * Luak runs its coroutines on Kotlin's own `Continuation` machinery rather than a thread apiece,
 * so a function written by the host can suspend and the interpreter unwinds to whoever entered it.
 * That is what lets a plugin write `wait(20)` in the middle of a command instead of a callback: the
 * turn ends there and the rest of it is picked up by whichever owner task resumes it.
 *
 * Entering through [start] rather than `call()` is deliberate: the synchronous entry points cannot
 * pause, and a plugin that tries to wait inside one gets the C-call boundary error instead.
 */
public class LuaCall internal constructor(
    private val sandbox: LuaSandbox,
) {
    /**
     * Runs [fn] until it finishes or pauses, and calls [onDone] when it finishes.
     *
     * [onDone] carries what went wrong, or null; it may be called before this returns, when the
     * call never pauses at all, which is the common case.
     */
    public fun start(
        fn: LuaValue,
        args: Varargs = Lua.NONE,
        onDone: (LuaFailure?) -> Unit = {},
    ) {
        val body: suspend () -> Unit = { fn.invokeSuspend(args) }
        sandbox.refill()
        body.startCoroutine(
            Continuation(EmptyCoroutineContext) { result ->
                onDone(result.exceptionOrNull()?.let { it.asFailure(sandbox.name) })
            },
        )
    }
}

/** How a plugin enters Lua: one of these per state. */
public fun LuaSandbox.calls(): LuaCall = LuaCall(this)

/**
 * Pauses the plugin here and comes back when [arrange] says so.
 *
 * The host is handed a `resume`; calling it continues the plugin **on the caller's thread**, so it
 * is called from the owner that is allowed to run this plugin and nowhere else. Never calling it
 * drops the rest of the turn, which is how an unloaded plugin stops without anything to kill.
 */
public suspend fun pauseUntil(arrange: (resume: () -> Unit) -> Unit): Unit =
    suspendCoroutine { continuation ->
        arrange { continuation.resume(Unit) }
    }

/**
 * A function of the host that a plugin may wait on.
 *
 * Called from somewhere that cannot pause (a `table.sort` comparator, a metamethod, anything the
 * interpreter reached through its synchronous path) it says so rather than blocking a tick.
 */
public abstract class WaitingFunction(
    private val functionName: String,
) : VarArgFunction() {
    final override suspend fun invokeSuspend(args: Varargs): Varargs = waiting(args)

    final override fun invoke(args: Varargs): Varargs =
        throw LuaError("'$functionName' cannot be used from here: it needs to be able to wait")

    protected abstract suspend fun waiting(args: Varargs): Varargs
}

/** A function of the host that answers straight away. */
public class HostFunction(
    private val functionName: String,
    private val body: (Varargs) -> Varargs,
) : VarArgFunction() {
    override fun invoke(args: Varargs): Varargs = body(args)

    override fun tojstring(): String = "function: $functionName"
}

internal fun Throwable.asFailure(plugin: String): LuaFailure =
    when (this) {
        is LuaFailure -> this
        is LuaError -> LuaFailure(plugin, this)
        else -> LuaFailure(plugin, LuaError(this))
    }
