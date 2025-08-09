package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.WaitingFunction
import org.basaltmc.server.lua.pauseUntil

/** `wait(ticks)`: ends the turn here and picks it up again later. */
internal class Wait(
    private val host: PluginHost,
    private val plugin: LoadedPlugin,
    private val instance: PluginInstance,
) : WaitingFunction("wait") {
    override suspend fun waiting(args: Varargs): Varargs {
        if (instance.deciderDepth > 0) {
            throw net.blueva.luak.LuaError(
                "wait cannot be used inside on(): the server is holding the question open for " +
                    "the answer. Use watch(), which runs after the fact and may take its time",
            )
        }
        val ticks = args.optint(1, 1).coerceAtLeast(0)
        pauseUntil { resume ->
            val handle =
                host.services.tasks.after(ticks) {
                    instance.sandbox.refill()
                    host.resuming(plugin) { resume() }
                }
            plugin.registrations.add("wait($ticks)") { handle.cancel() }
        }
        return Lua.NONE
    }
}

internal fun later(
    host: PluginHost,
    plugin: LoadedPlugin,
    instance: PluginInstance,
): LuaValue =
    HostFunction("later") { args ->
        val ticks = args.checkint(1)
        val fn = args.arg(2)
        if (!fn.isfunction()) throw luaError("later(ticks, fn) needs a function")
        val handle =
            host.services.tasks.after(ticks) {
                host.enter(plugin, instance, fn, "later")
            }
        handleTable(plugin.registrations.add("later($ticks)") { handle.cancel() })
    }

internal fun every(
    host: PluginHost,
    plugin: LoadedPlugin,
    instance: PluginInstance,
): LuaValue =
    HostFunction("every") { args ->
        // every(period, fn) is the common shape; every(delay, period, fn) when the first one
        // should not be immediate.
        val threeArguments = args.narg() >= 3
        val delay = if (threeArguments) args.checkint(1) else args.checkint(1)
        val period = if (threeArguments) args.checkint(2) else args.checkint(1)
        val fn = if (threeArguments) args.arg(3) else args.arg(2)
        if (!fn.isfunction()) throw luaError("every(period, fn) needs a function")
        val handle =
            host.services.tasks.every(delay, period) {
                host.enter(plugin, instance, fn, "every")
            }
        handleTable(plugin.registrations.add("every($period)") { handle.cancel() })
    }
