package org.basaltmc.common.logging

import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * A minimal, dependency-free leveled logger. Writes to stdout via [println]; there is no
 * separate stderr sink or file appender yet, that lands when a system actually needs it.
 *
 * Every line has the same fixed shape, `[yyyy-MM-dd HH:mm:ss] [LEVEL] [name]: message`, in UTC
 * (matching [kotlin.time.Clock.System], not the host's local timezone: getting local time
 * cross-platform on Kotlin/Native needs a `cinterop` call this project doesn't have yet, and
 * UTC is unambiguous in the meantime).
 *
 * Message producers are `inline` so a filtered-out call (below [minLevel]) never builds the
 * string or allocates a closure.
 */
public class Logger(
    @PublishedApi internal val name: String,
) {
    public var minLevel: LogLevel = defaultMinLevel

    public inline fun trace(message: () -> String) {
        log(LogLevel.TRACE, message)
    }

    public inline fun debug(message: () -> String) {
        log(LogLevel.DEBUG, message)
    }

    public inline fun info(message: () -> String) {
        log(LogLevel.INFO, message)
    }

    public inline fun warn(message: () -> String) {
        log(LogLevel.WARN, message)
    }

    public inline fun error(message: () -> String) {
        log(LogLevel.ERROR, message)
    }

    public inline fun error(
        throwable: Throwable,
        message: () -> String,
    ) {
        log(LogLevel.ERROR) { "${message()}: ${throwable::class.simpleName}: ${throwable.message}" }
    }

    @PublishedApi
    internal inline fun log(
        level: LogLevel,
        message: () -> String,
    ) {
        if (level.ordinal < minLevel.ordinal) return
        println("[${timestamp()}] [${level.label}] [$name]: ${message()}")
    }

    public companion object {
        /** Applied to every [Logger] created after it is set; existing instances keep whatever [minLevel] they already had. */
        public var defaultMinLevel: LogLevel = LogLevel.INFO

        @OptIn(ExperimentalTime::class)
        @PublishedApi
        internal fun timestamp(): String {
            // kotlin.time.Instant.toString() is fixed ISO-8601 ("yyyy-MM-ddTHH:mm:ss[.fraction]Z"),
            // so slicing out the date and whole-second time is simpler and just as reliable as
            // decomposing the instant into calendar fields by hand.
            val iso = Clock.System.now().toString()
            return "${iso.substring(0, 10)} ${iso.substring(11, 19)}"
        }
    }
}
