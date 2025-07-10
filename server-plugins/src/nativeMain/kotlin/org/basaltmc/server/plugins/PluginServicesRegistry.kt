package org.basaltmc.server.plugins

import net.blueva.luak.LuaError
import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * What one plugin offers another, and the only way across.
 *
 * A service looks like a table of functions and is not one: a Lua table belongs to the state that
 * made it. What crosses is a call the host mediates, so the provider's function runs in the
 * provider's state and on its own budget. Values are copied, which is why they are numbers, strings,
 * booleans and tables of those; a player crosses as a uuid and is looked up on the other side.
 */
internal class PluginServiceRegistry {
    private class Offer(
        val pluginId: String,
        val functions: Map<String, LuaValue>,
    )

    private val offers = HashMap<String, Offer>()

    /** What is on offer, for `/plugins` and for a plugin asking what is there. */
    val names: Set<String> get() = offers.keys

    fun provide(
        pluginId: String,
        name: String,
        table: LuaValue,
    ): PluginHandle {
        if (!table.istable()) throw LuaError("service('$name', ...) needs a table of functions")
        val functions = mutableMapOf<String, LuaValue>()
        var key: LuaValue = Lua.NIL
        while (true) {
            val pair = table.next(key) ?: break
            val nextKey = pair.arg1()
            if (nextKey.isnil()) break
            val value = pair.arg(2)
            if (value.isfunction()) functions[nextKey.tojstring()] = value
            key = nextKey
        }
        if (functions.isEmpty()) throw LuaError("service('$name', ...) has no functions in it")
        offers[name] = Offer(pluginId, functions)
        return PluginHandle { if (offers[name]?.pluginId == pluginId) offers.remove(name) }
    }

    /**
     * The table a consumer gets: one host function per function the provider offered.
     *
     * Null when nobody offers it, which is what lets `if bank then` be the whole of a soft
     * dependency.
     */
    fun consume(
        host: PluginHost,
        name: String,
    ): LuaTable? {
        val offer = offers[name] ?: return null
        val provider = host.pluginById(offer.pluginId) ?: return null
        val table = Lua.table()
        for ((functionName, function) in offer.functions) {
            table.set(
                functionName,
                HostFunction("$name.$functionName") { args ->
                    host.callService(provider, function, args)
                },
            )
        }
        return table
    }
}

/**
 * Values that may cross between two plugins: copies, never the thing itself.
 *
 * A player crosses as their uuid and is looked up on the other side, so what the provider holds is
 * its own handle rather than one made in somebody else's state.
 */
internal object PluginValues {
    private const val PLAYER_MARK = "__player_uuid"

    fun out(
        value: LuaValue,
        depth: Int = 0,
    ): Any? =
        when {
            value.isnil() -> null
            value.isboolean() -> value.toboolean()
            value.isint() -> value.tolong()
            value.isnumber() -> value.todouble()
            value.isstring() -> value.tojstring()
            value.istable() && depth < MAX_DEPTH -> table(value, depth)
            else -> null
        }

    private fun table(
        value: LuaValue,
        depth: Int,
    ): Any? {
        // A player object is recognised by what it answers, not by its identity: it is a table with
        // a uuid, and that uuid is all the other side needs.
        val uuid = (value.get("uuid") ?: Lua.NIL).takeIf { it.isstring() }?.tojstring()
        val name = (value.get("name") ?: Lua.NIL).takeIf { it.isstring() }?.tojstring()
        if (uuid != null && name != null) return mapOf(PLAYER_MARK to uuid)
        val out = mutableMapOf<String, Any?>()
        var key: LuaValue = Lua.NIL
        while (true) {
            val pair = value.next(key) ?: break
            val nextKey = pair.arg1()
            if (nextKey.isnil()) break
            out[nextKey.tojstring()] = out(pair.arg(2), depth + 1)
            key = nextKey
        }
        return out
    }

    fun into(
        value: Any?,
        host: PluginHost,
        plugin: LoadedPlugin,
    ): LuaValue =
        when (value) {
            null -> Lua.NIL
            is Boolean -> Lua.of(value)
            is Long -> Lua.of(value)
            is Double -> Lua.of(value)
            is String -> Lua.of(value)
            is Map<*, *> -> {
                val uuid = value[PLAYER_MARK] as? String
                if (uuid != null) {
                    host.services.players
                        .byUuid(uuid)
                        ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) }
                        ?: Lua.NIL
                } else {
                    val out = Lua.table()
                    for ((key, element) in value) out.set(key.toString(), into(element, host, plugin))
                    out
                }
            }

            else -> Lua.of(value.toString())
        }

    fun arguments(args: Varargs): List<Any?> = (1..args.narg()).map { out(args.arg(it)) }

    private const val MAX_DEPTH = 8
}
