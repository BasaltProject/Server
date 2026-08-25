package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/** Registers a plugin's commands with the server's own dispatcher, and runs them when called. */
internal class PluginCommandDispatch(
    private val host: PluginHost,
) {
    /** Registers one command once, and remembers which function each lane answers it with. */
    internal fun registerCommand(
        plugin: LoadedPlugin,
        instance: PluginInstance,
        spec: PluginCommandSpec,
        handler: LuaValue,
    ) {
        val key = spec.literals.joinToString(" ")
        val existing = plugin.commands[key]
        if (existing != null) {
            existing.handlers[instance.lane] = handler
            return
        }
        val entry = CommandEntry(spec)
        entry.handlers[instance.lane] = handler
        plugin.commands[key] = entry
        val handle =
            host.services.commands.register(plugin.manifest.id, spec) { call ->
                runCommand(plugin, entry, call)
            }
        plugin.registrations.add("command /${spec.usage}") {
            handle.cancel()
            plugin.commands.remove(key)
        }
    }

    private fun runCommand(
        plugin: LoadedPlugin,
        entry: CommandEntry,
        call: PluginCommandCall,
    ) {
        val instance = laneFor(plugin, entry) ?: return
        val handler = entry.handlers[instance.lane] ?: return
        val arguments = mutableListOf<LuaValue>(sourceTable(plugin, call))
        for (argument in entry.spec.arguments) arguments += luaOf(call.argument(argument.name))
        host.enter(plugin, instance, handler, "/${entry.spec.name}", Lua.varargs(*arguments.toTypedArray()))
    }

    /** The lane that answers the next fire of this event type, the same way a command picks one. */
    internal fun laneForEvent(plugin: LoadedPlugin): PluginInstance? = plugin.instances.firstOrNull()

    /**
     * Which lane answers this one.
     *
     * With one lane there is nothing to decide. With more, they take turns, which is on purpose: a
     * plugin that assumed one state sees it here, in a test, rather than in production the first
     * time an operator turns regions on.
     */
    private fun laneFor(
        plugin: LoadedPlugin,
        entry: CommandEntry,
    ): PluginInstance? {
        if (plugin.instances.isEmpty()) return null
        val lane = entry.nextLane % plugin.instances.size
        entry.nextLane = lane + 1
        return plugin.instances.firstOrNull { it.lane == lane } ?: plugin.instances.first()
    }

    private fun sourceTable(
        plugin: LoadedPlugin,
        call: PluginCommandCall,
    ): LuaValue {
        val table = Lua.table()
        table.set("name", call.source)
        table.set("is_console", Lua.of(call.isConsole))
        table.set(
            "send",
            HostFunction("send") { args ->
                // Both `src:send(text)` and `src.send(text)` end up here; the first passes the
                // table as well, and a plugin should not have to care which it wrote.
                val first = firstArgument(args)
                call.reply(plugin.text.resolve(args.tojstring(first), args.arg(first + 1)))
                Lua.NONE
            },
        )
        return table
    }

    private fun luaOf(value: Any?): LuaValue =
        when (value) {
            null -> Lua.NIL
            is String -> Lua.of(value)
            is Long -> Lua.of(value)
            is Int -> Lua.of(value)
            is Double -> Lua.of(value)
            is Boolean -> Lua.of(value)
            else -> Lua.of(value.toString())
        }
}
