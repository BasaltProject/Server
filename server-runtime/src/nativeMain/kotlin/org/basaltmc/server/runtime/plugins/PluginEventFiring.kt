package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginEvent
import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventType

/**
 * How the server fires an event, and what it costs when nobody is listening.
 *
 * Inline and null-tolerant on purpose: with no bus, or with no handler for this type, the whole
 * thing is a null check and a boolean, the values are never worked out and nothing is allocated.
 * That is the promise the event catalogue makes to the tick loop, kept in one place instead of
 * being re-typed at every call site.
 *
 * Returns the event when it actually fired, so the caller can look at `cancelled` or read back a
 * field a handler changed, and null when there was nobody to tell.
 */
internal inline fun PluginEventBus?.emit(
    type: PluginEventType,
    fill: (PluginEvent) -> Unit,
): PluginEvent? {
    val bus = this ?: return null
    if (!bus.listening(type)) return null
    val event = bus.event(type)
    fill(event)
    bus.fire(event)
    return event
}
