package org.basaltmc.server.plugins

import net.blueva.luak.LuaError
import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

/**
 * What a handler receives: a table that reads straight from the event rather than a copy of it.
 *
 * One of these per event type and lane, made when the first handler is registered and reused for
 * every fire, so dispatching does not build a table. Reading a field goes through `__index` to the
 * event itself; writing one goes through `__newindex`, which refuses anything the type did not
 * declare mutable instead of quietly keeping a value the server will never read.
 *
 * It also refuses to work once the handler has returned. That is not tidiness: an event object is
 * reused, so a plugin that stashed one and read it a tick later would be reading somebody else's
 * event, and the value would look perfectly reasonable.
 */
internal class PluginEventView(
    private val event: PluginEvent,
    private val onCancel: (String?) -> Unit,
    /**
     * Turns a name the event carries into the object behind it: `player` from `player_name`,
     * `entity` from `entity_id`.
     *
     * Looked up rather than carried, and only when asked for: an event that fires every tick should
     * not build an object nobody reads.
     */
    private val objects: (String) -> LuaValue? = { null },
    /** A refusal is text like any other, so `@key` and `{name}` work in it too. */
    private val resolve: (String, LuaValue?) -> String = { message, _ -> message },
) {
    private var generation: Long = -1

    val table: LuaTable = Lua.table()

    init {
        val metatable = Lua.table()
        metatable.set("__index", HostFunction("event.index") { args -> read(args) })
        metatable.set("__newindex", HostFunction("event.newindex") { args -> write(args) })
        // Nothing of the event is in the table itself, so both metamethods always run.
        table.setmetatable(metatable)
    }

    /** Ties the view to this fire; anything held from an earlier one is stale from here on. */
    fun bind() {
        generation = event.generation
    }

    private fun check() {
        if (generation != event.generation || !event.live) {
            throw LuaError(
                "this ${event.type.name} event is over: it cannot be read or changed after the handler returns",
            )
        }
    }

    private fun read(args: Varargs): Varargs {
        val key = args.arg(2).tojstring()
        if (key == "cancel") return cancelFunction
        if (key == "name") return Lua.of(event.type.name)
        if (key == "cancelled") {
            check()
            return Lua.of(event.cancelled)
        }
        check()
        objects(key)?.let { return it }
        val index = event.type.indexOf(key)
        if (index < 0) {
            throw LuaError(
                "a ${event.type.name} event has no '$key'; it has ${event.type.fields.joinToString(", ")}" +
                    (if (event.type.fields.contains("player_name")) " (and 'player')" else "") +
                    (if (event.type.fields.contains("entity_id")) " (and 'entity')" else "") +
                    (if (event.type.fields.contains("x")) " (and 'pos')" else ""),
            )
        }
        return valueOf(event.get(index))
    }

    private fun write(args: Varargs): Varargs {
        check()
        val key = args.arg(2).tojstring()
        if (key !in event.type.mutable) {
            val allowed = if (event.type.mutable.isEmpty()) "nothing" else event.type.mutable.joinToString(", ")
            throw LuaError("'$key' cannot be changed on a ${event.type.name} event; it lets you change $allowed")
        }
        event.set(key, fromLua(args.arg(3)))
        return Lua.NONE
    }

    private val cancelFunction: LuaValue =
        HostFunction("event.cancel") { args ->
            check()
            if (!event.type.cancellable) {
                throw LuaError("a ${event.type.name} event cannot be cancelled: it is telling you, not asking")
            }
            // Both `e:cancel()` and `e:cancel(message)` land here, and `e.cancel()` too.
            val first = firstArgument(args)
            val written = if (args.narg() >= first && args.isstring(first)) args.tojstring(first) else null
            onCancel(written?.let { resolve(it, args.arg(first + 1)) })
            Lua.NONE
        }

    private fun valueOf(value: Any?): LuaValue =
        when (value) {
            null -> Lua.NIL
            is String -> Lua.of(value)
            is Int -> Lua.of(value)
            is Long -> Lua.of(value)
            is Double -> Lua.of(value)
            is Float -> Lua.of(value.toDouble())
            is Boolean -> Lua.of(value)
            else -> Lua.of(value.toString())
        }

    private fun fromLua(value: LuaValue): Any? =
        when {
            value.isnil() -> null
            value.isboolean() -> value.toboolean()
            value.isint() -> value.tolong()
            value.isnumber() -> value.todouble()
            else -> value.tojstring()
        }
}
