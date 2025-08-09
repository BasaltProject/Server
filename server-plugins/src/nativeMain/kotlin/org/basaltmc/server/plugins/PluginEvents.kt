package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * `on(event, [opts], fn)` and `watch(event, [opts], fn)`.
 *
 * The two lifecycle names stay on `on`: they are about this plugin, not about the world.
 *
 * Everything in `opts` is optional: `priority`, `cancelled = true` to be told about events
 * another handler stopped, and any other key filters on a field of the event, which is what
 * saves every handler from opening with the same `if`.
 */
internal fun listener(
    host: PluginHost,
    plugin: LoadedPlugin,
    instance: PluginInstance,
    observer: Boolean,
): LuaValue =
    HostFunction(if (observer) "watch" else "on") { args ->
        val name = args.checkjstring(1)
        val hasOptions = args.narg() >= 3
        val options = if (hasOptions) args.arg(2) else Lua.NIL
        val fn = if (hasOptions) args.arg(3) else args.arg(2)
        if (!fn.isfunction()) throw luaError("${if (observer) "watch" else "on"}(event, fn) needs a function")

        if (name == PluginEventTypes.PLUGIN_ENABLE || name == PluginEventTypes.PLUGIN_DISABLE) {
            if (observer) throw luaError("'$name' is about this plugin: listen to it with on(), not watch()")
            if (name == PluginEventTypes.PLUGIN_ENABLE) instance.onEnable += fn else instance.onDisable += fn
            return@HostFunction Lua.NIL
        }

        // A name with a colon in it is a plugin's own, not one of the server's.
        if (':' in name) {
            if (observer) throw luaError("'$name' belongs to a plugin: listen to it with on()")
            host.announce(plugin, name, fn)
            return@HostFunction Lua.NIL
        }

        val type =
            host.events.typeOf(name)
                ?: throw luaError(
                    "there is no event called '$name'; this server has " +
                        host.events.declared.joinToString(", ") { it.name },
                )
        if (!observer && !type.cancellable && options.istable() && !(options.get("cancelled") ?: Lua.NIL).isnil()) {
            throw luaError("'$name' cannot be cancelled, so 'cancelled' means nothing on it")
        }

        var priority = PluginEventPriority.NORMAL
        var wantsCancelled = false
        val filters = mutableMapOf<String, Any?>()
        if (options.istable()) {
            for (field in type.fields + listOf("priority", "cancelled")) {
                val value = options.get(field) ?: Lua.NIL
                if (value.isnil()) continue
                when (field) {
                    "priority" ->
                        priority =
                            PluginEventPriority.of(value.tojstring())
                                ?: throw luaError(
                                    "'${value.tojstring()}' is not a priority; they are " +
                                        PluginEventPriority.entries.joinToString(", ") { it.name.lowercase() },
                                )

                    "cancelled" -> wantsCancelled = value.toboolean()
                    else -> filters[field] = fromLua(value)
                }
            }
        }
        host.registerEventHandler(plugin, instance, type, priority, observer, wantsCancelled, filters, fn)
        Lua.NIL
    }

internal fun fromLua(value: LuaValue): Any? =
    when {
        value.isnil() -> null
        value.isboolean() -> value.toboolean()
        value.isint() -> value.tolong()
        value.isnumber() -> value.todouble()
        else -> value.tojstring()
    }

/** `emit("arena:round_start", { ... })`: tell whoever is listening, and carry on either way. */
internal fun emit(host: PluginHost): LuaValue =
    HostFunction("emit") { args ->
        val name = args.checkjstring(1)
        if (':' !in name) {
            throw luaError(
                "'$name' has no colon in it, so it would collide with the server's own events; " +
                    "call it something like 'yourplugin:$name'",
            )
        }
        Lua.of(host.emit(name, args.arg(2)))
    }
