package org.basaltmc.common.logging

/**
 * A minimal, dependency-free leveled logger. Writes to stdout via [println]; there is no
 * separate stderr sink or file appender yet, that lands when a system actually needs it.
 *
 * Message producers are `inline` so a filtered-out call (below [minLevel]) never builds the
 * string or allocates a closure.
 */
public class Logger(
    @PublishedApi internal val name: String,
) {
    public var minLevel: LogLevel = LogLevel.INFO

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
        println("[${level.label}] $name: ${message()}")
    }
}
