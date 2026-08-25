package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.LuaFailure

/** Events a plugin made up, by name. Nothing declares them: whoever emits one decides it exists. */
internal class PluginAnnouncements {
    /** One plugin waiting on an event another plugin made up. */
    private class Announcement(
        val plugin: LoadedPlugin,
        val fn: LuaValue,
    )

    private val announcements = LinkedHashMap<String, MutableList<Announcement>>()

    /**
     * `on("arena:round_start", fn)` for a name no event type has.
     *
     * The loosest of the ways to talk: the one emitting does not know whether anybody
     * is there, and the one listening does not need the other to be loaded, or ever to exist.
     */
    internal fun announce(
        plugin: LoadedPlugin,
        name: String,
        fn: LuaValue,
    ) {
        val list = announcements.getOrPut(name) { mutableListOf() }
        val entry = Announcement(plugin, fn)
        list += entry
        plugin.registrations.add("on($name)") {
            list.remove(entry)
            if (list.isEmpty()) announcements.remove(name)
        }
    }

    /**
     * `emit("arena:round_start", { ... })`.
     *
     * The table crosses by copy, the same as a service: what reaches each listener is its
     * own, so two plugins cannot end up holding the same mutable table, and each one's work is paid
     * by its own budget.
     */
    internal fun emit(
        host: PluginHost,
        name: String,
        payload: LuaValue,
    ): Int {
        val listeners = announcements[name] ?: return 0
        val copied = PluginValues.out(payload)
        var reached = 0
        for (entry in listeners.toList()) {
            val plugin = entry.plugin
            if (plugin.disabled) continue
            val instance = plugin.instances.firstOrNull() ?: continue
            instance.sandbox.refill()
            instance.deciderDepth++
            try {
                entry.fn.call(PluginValues.into(copied, host, plugin))
                plugin.strikes = 0
                reached++
            } catch (e: net.blueva.luak.LuaError) {
                host.report(plugin, LuaFailure(plugin.manifest.id, e), name)
            } finally {
                instance.deciderDepth--
            }
        }
        return reached
    }
}
