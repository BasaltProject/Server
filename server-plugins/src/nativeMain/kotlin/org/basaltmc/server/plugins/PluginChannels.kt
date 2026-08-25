package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * `channel.on("arena:main", fn)` and `channel.send(player, "arena:main", bytes)`.
 *
 * Only the channels the manifest listed under `[capabilities] network`: a plugin listening on
 * `bungeecord:main` is talking to the proxy, and that is the operator's decision, not the
 * plugin's. Asking for one that was not declared is refused with the list that was.
 */
internal fun channel(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue {
    val allowed = plugin.manifest.capabilities.network

    fun check(name: String) {
        if (name in allowed) return
        throw luaError(
            if (allowed.isEmpty()) {
                "${plugin.manifest.id} declared no channels; add network = [\"$name\"] " +
                    "to [capabilities] in plugin.toml"
            } else {
                "'$name' is not one of ${plugin.manifest.id}'s channels, which are " +
                    allowed.joinToString(", ")
            },
        )
    }

    val table = Lua.table()
    table.set(
        "on",
        HostFunction("channel.on") { args ->
            val name = args.checkjstring(1)
            check(name)
            val fn = args.arg(2)
            if (!fn.isfunction()) throw luaError("channel.on(name, fn) needs a function")
            host.listenOn(plugin, name, fn)
            Lua.NIL
        },
    )
    table.set(
        "send",
        HostFunction("channel.send") { args ->
            val player =
                playerFrom(args.arg(1), host.services.players)
                    ?: throw luaError("channel.send(player, name, bytes) needs a player")
            val name = args.checkjstring(2)
            check(name)
            val body = args.arg(3)
            if (!body.istable()) throw luaError("channel.send needs the body as a list of bytes")
            val bytes =
                ByteArray(body.length()) { index ->
                    ((body.get(index + 1) ?: Lua.NIL).toint() and 0xff).toByte()
                }
            Lua.of(host.services.channels.send(player, name, bytes))
        },
    )
    return table
}
