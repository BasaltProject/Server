package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

internal fun command(
    host: PluginHost,
    plugin: LoadedPlugin,
    instance: PluginInstance,
): LuaValue =
    HostFunction("cmd") { args ->
        val spec = args.checkjstring(1)
        val fn = args.arg(2)
        if (!fn.isfunction()) throw luaError("cmd(spec, fn) needs a function")
        val options = args.arg(3)
        val permission =
            if (options.istable()) {
                (options.get("permission") ?: Lua.NIL).let { if (it.isstring()) it.tojstring() else null }
            } else {
                null
            }
        val parsed =
            try {
                PluginCommandSpec.parse(spec, permission)
            } catch (e: ManifestException) {
                throw luaError(e.message ?: "bad command")
            }
        host.registerCommand(plugin, instance, parsed, fn)
        Lua.NIL
    }

/**
 * `run("say hello")`: one command, as the console would.
 *
 * As the console and never as a player, so a plugin cannot borrow somebody's permissions by
 * running something in their name.
 */
internal fun runCommand(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue =
    HostFunction("run") { args ->
        val command = args.checkjstring(1)
        plugin.log.info { "ran /$command" }
        Lua.of(host.services.commands.runAsConsole(command))
    }
