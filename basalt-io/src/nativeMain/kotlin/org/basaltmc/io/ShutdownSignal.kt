@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.staticCFunction
import platform.posix.SIGINT
import platform.posix.SIGTERM
import platform.posix.signal
import kotlin.concurrent.AtomicInt

/**
 * Turns `Ctrl+C` and `kill` into an orderly shutdown instead of an abrupt death.
 *
 * A signal handler may only call async-signal-safe functions: allocating, taking a lock or writing
 * to a file from inside one can deadlock against whatever the interrupted thread was doing. So the
 * handler does exactly one thing, set a flag, and the real work happens on a normal thread that
 * notices it. [requested] is created when this object is first touched, which [install] forces
 * before any signal can arrive, so the handler itself never triggers initialization.
 *
 * Both signals matter and for different reasons: `SIGINT` is an operator pressing Ctrl+C, `SIGTERM`
 * is what `systemd`, Docker and every process supervisor send first before resorting to `SIGKILL`.
 * Without this the server lost up to one save interval of every player on any of those.
 */
public object ShutdownSignal {
    private val requested = AtomicInt(0)

    /** True once a termination signal has arrived. Polled; never acted on from the handler itself. */
    public val isRequested: Boolean get() = requested.value != 0

    public fun install() {
        // Touches the flag before installing, so it is already initialized when a signal lands.
        requested.value = 0
        val handler = staticCFunction<Int, Unit> { _ -> ShutdownSignal.markRequested() }
        signal(SIGINT, handler)
        signal(SIGTERM, handler)
    }

    private fun markRequested() {
        requested.value = 1
    }
}
