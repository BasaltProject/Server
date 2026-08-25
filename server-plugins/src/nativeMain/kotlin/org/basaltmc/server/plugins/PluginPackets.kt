package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field

/**
 * `packet.on(name, fn)`, `packet.watch(name, fn)` and `packet.send(player, name, bytes)`.
 *
 * The names are the protocol's own with the direction in front, so `clientbound.set_title_text`
 * is what the jar calls `set_title_text` going out. A misspelling is refused at registration
 * with the list, rather than becoming a listener that never fires.
 */
internal fun packets(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue {
    val table = Lua.table()
    for (observer in listOf(false, true)) {
        table.set(
            if (observer) "watch" else "on",
            HostFunction(if (observer) "packet.watch" else "packet.on") { args ->
                val name = args.checkjstring(1)
                val hasOptions = args.narg() >= 3
                val options = if (hasOptions) args.arg(2) else Lua.NIL
                val fn = if (hasOptions) args.arg(3) else args.arg(2)
                if (!fn.isfunction()) throw luaError("packet.on(name, fn) needs a function")
                val sender = host.services.packets
                if (!sender.isKnown(name)) {
                    throw luaError(
                        "this server has no packet called '$name'. " +
                            "Names look like clientbound.set_title_text; /plugins packets lists them",
                    )
                }
                val priority =
                    options
                        .takeIf { it.istable() }
                        ?.field("priority")
                        ?.takeIf { it.isstring() }
                        ?.let { spelled ->
                            PluginEventPriority.of(spelled.tojstring())
                                ?: throw luaError("'${spelled.tojstring()}' is not a priority")
                        } ?: PluginEventPriority.NORMAL
                val handle = host.packets.add(plugin, name, fn, priority, observer)
                plugin.registrations.add("packet.${if (observer) "watch" else "on"}($name)") { handle.cancel() }
                Lua.NIL
            },
        )
    }
    table.set(
        "send",
        HostFunction("packet.send") { args ->
            val player =
                playerFrom(args.arg(1), host.services.players)
                    ?: throw luaError("packet.send(player, name, bytes) needs a player")
            val name = args.checkjstring(2)
            val body = args.arg(3)
            if (!body.istable()) throw luaError("packet.send needs the body as a list of bytes")
            val bytes =
                ByteArray(body.length()) { index ->
                    ((body.get(index + 1) ?: Lua.NIL).toint() and 0xff).toByte()
                }
            Lua.of(host.services.packets.send(player, name, bytes))
        },
    )
    table.set("names", HostFunction("packet.names") { listOf(host.services.packets.names()).toLuaList() })
    return table
}

internal fun List<List<String>>.toLuaList(): LuaValue {
    val out = Lua.table()
    for ((index, name) in first().withIndex()) out.set(index + 1, Lua.of(name))
    return out
}
