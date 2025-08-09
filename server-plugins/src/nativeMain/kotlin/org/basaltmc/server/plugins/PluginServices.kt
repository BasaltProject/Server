package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * `service("economy", { ... })` to offer, `service("economy")` to use.
 *
 * What comes back from the second is not the table the first passed: it is a set of calls the
 * host mediates, so the provider's code runs in the provider's state and on its budget. Null
 * when nobody offers it, which is the whole of a soft dependency.
 */
internal fun service(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue =
    HostFunction("service") { args ->
        val name = args.checkjstring(1)
        val offered = args.arg(2)
        if (offered.isnil()) {
            host.serviceRegistry.consume(host, name) ?: Lua.NIL
        } else {
            // Declared on both sides: what a plugin offers is in its manifest, so the
            // tooling knows it and the operator can read it without running anything.
            if (!plugin.manifest.exports.services
                    .contains(name)
            ) {
                throw luaError(
                    "$name is not in [exports] services of ${plugin.manifest.id}; " +
                        "add services = [\"$name\"] to plugin.toml",
                )
            }
            val handle = host.serviceRegistry.provide(plugin.manifest.id, name, offered)
            plugin.registrations.add("service($name)") { handle.cancel() }
            Lua.NIL
        }
    }

/** `t("welcome", { name = ... })`: the translation on its own, for text that goes somewhere else. */
internal fun translate(plugin: LoadedPlugin): LuaValue =
    HostFunction("t") { args ->
        Lua.of(plugin.text.resolve("@" + args.checkjstring(1), args.arg(2)))
    }
