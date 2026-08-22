package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue

/**
 * Who is watching which packet.
 *
 * The same two verbs as the event bus and the same five priorities: `packet.on` decides and may
 * cancel, `packet.watch` only looks. What is different is the promise underneath: with nobody
 * listening to a name, [listening] is a map lookup and the packet is never opened, never copied and
 * never handed to Lua. That is the whole reason this can sit on the path every packet takes.
 */
public class PluginPacketRegistry internal constructor(
    private val deliver: (LoadedPlugin, LuaValue, PluginPacket) -> Unit,
) : PluginPacketBus {
    private class Listener(
        val plugin: LoadedPlugin,
        val fn: LuaValue,
        val priority: PluginEventPriority,
        val observer: Boolean,
    )

    private val listeners = HashMap<String, MutableList<Listener>>()
    private val reusable = PluginPacket()

    internal fun add(
        plugin: LoadedPlugin,
        name: String,
        fn: LuaValue,
        priority: PluginEventPriority,
        observer: Boolean,
    ): PluginHandle {
        val list = listeners.getOrPut(name) { mutableListOf() }
        val entry = Listener(plugin, fn, priority, observer)
        list += entry
        // The same order the event bus uses: deciders first in priority order, observers after, so
        // whoever only looks sees what was settled.
        list.sortWith(compareBy({ if (it.observer) 1 else 0 }, { it.priority.ordinal }))
        return PluginHandle {
            list.remove(entry)
            if (list.isEmpty()) listeners.remove(name)
        }
    }

    public override fun listening(name: String): Boolean = listeners.containsKey(name)

    public override fun packet(): PluginPacket = reusable

    public override fun fire(packet: PluginPacket): Boolean {
        val list = listeners[packet.name] ?: return true
        packet.clear()
        for (listener in list.toList()) {
            if (packet.cancelled && !listener.observer) continue
            deliver(listener.plugin, listener.fn, packet)
        }
        return !packet.cancelled
    }
}
