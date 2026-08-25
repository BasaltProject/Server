package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/** `placeholder("wins", fn)` registers `%<id>_wins%` in the server's own registry. */
internal fun placeholder(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue =
    HostFunction("placeholder") { args ->
        val name = args.checkjstring(1)
        val fn = args.arg(2)
        if (!fn.isfunction()) throw luaError("placeholder(name, fn) needs a function")
        val handle =
            host.services.placeholders.register(plugin.manifest.id, name) { player ->
                host.answer(plugin, fn, player)
            }
        plugin.registrations.add("placeholder(%${plugin.manifest.id}_$name%)") { handle.cancel() }
        Lua.NIL
    }

/** `fill(text, player)`: renders every placeholder the server knows, not only this plugin's. */
internal fun fill(host: PluginHost): LuaValue =
    HostFunction("fill") { args ->
        val text = args.checkjstring(1)
        val player = args.arg(2)
        Lua.of(host.services.placeholders.fill(text, host.playerOf(player)))
    }

/** `broadcast(text)`: to everyone online, MiniMessage like everything else. */
internal fun broadcast(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue =
    HostFunction("broadcast") { args ->
        val message = plugin.text.resolve(args.checkjstring(1), args.arg(2))
        for (player in host.services.players.online()) player.send(message)
        Lua.NONE
    }
