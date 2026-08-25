package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field
import org.basaltmc.server.lua.WaitingFunction
import org.basaltmc.server.lua.pauseUntil

/** `players.online()`, `players.by_name(name)`, `players.by_uuid(uuid)`. */
internal fun playersTable(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaTable {
    val table = Lua.table()
    table.set(
        "online",
        HostFunction("players.online") {
            val out = Lua.table()
            for ((index, player) in host.services.players
                .online()
                .withIndex()) {
                out.set(index + 1, playerObject(player, host.services.storage, plugin.manifest.id, plugin.text))
            }
            out
        },
    )
    table.set(
        "by_name",
        HostFunction("players.by_name") { args ->
            host.services.players
                .byName(args.checkjstring(1))
                ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL
        },
    )
    table.set(
        "by_uuid",
        HostFunction("players.by_uuid") { args ->
            host.services.players
                .byUuid(args.checkjstring(1))
                ?.let { playerObject(it, host.services.storage, plugin.manifest.id, plugin.text) } ?: Lua.NIL
        },
    )
    table.set(
        "count",
        HostFunction("players.count") {
            Lua.of(
                host.services.players
                    .online()
                    .size,
            )
        },
    )
    return table
}

/** `worlds.all()`, `worlds.by_name(name)`, `worlds.default()`. */
internal fun worldsTable(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaTable {
    val clicked = { fn: LuaValue, uuid: String -> host.npcClicked(plugin, fn, uuid) }
    val resolve = { written: String -> plugin.text.resolve(written, Lua.NIL) }
    // `worlds.create{ name = "arena", dimension = "overworld", type = "flat", seed = 1 }`.
    val make =
        HostFunction("worlds.create") { args ->
            val options = args.arg(1)
            if (!options.istable()) throw luaError("worlds.create{ name = ..., dimension = ... } takes a table")
            val name = options.field("name")
            if (!name.isstring()) throw luaError("a world needs a name")
            val seed = options.field("seed")
            host.services.worlds
                .create(
                    name.tojstring(),
                    options.field("dimension").takeIf { it.isstring() }?.tojstring() ?: "overworld",
                    options.field("type").takeIf { it.isstring() }?.tojstring() ?: "normal",
                    if (seed.isnumber()) seed.tolong() else null,
                )?.let { worldObject(it, clicked, resolve) } ?: Lua.NIL
        }
    val table = Lua.table()
    table.set("create", make)
    table.set(
        "all",
        HostFunction("worlds.all") {
            val out = Lua.table()
            for ((index, world) in host.services.worlds
                .all()
                .withIndex()) {
                out.set(index + 1, worldObject(world, clicked, resolve))
            }
            out
        },
    )
    table.set(
        "by_name",
        HostFunction("worlds.by_name") { args ->
            host.services.worlds
                .byName(args.checkjstring(1))
                ?.let { worldObject(it, clicked, resolve) } ?: Lua.NIL
        },
    )
    table.set(
        "default",
        HostFunction("worlds.default") {
            host.services.worlds.default
                ?.let { worldObject(it, clicked, resolve) } ?: Lua.NIL
        },
    )
    return table
}

/**
 * `at(world, x, z)`: from here on, this turn runs on the owner of that position.
 *
 * It is a pause, not a lock: the rest of the function is picked up by that owner when it gets
 * its turn, which is why it can only be used where pausing is allowed (not inside `on`).
 */
internal fun at(
    host: PluginHost,
    plugin: LoadedPlugin,
    instance: PluginInstance,
): LuaValue =
    object : WaitingFunction("at") {
        override suspend fun waiting(args: Varargs): Varargs {
            if (instance.deciderDepth > 0) {
                throw net.blueva.luak.LuaError(
                    "at() cannot be used inside on(): the server is holding the question open. " +
                        "Use watch(), which runs after the fact",
                )
            }
            // at(pos) and at(world, x, z) are the same question asked two ways.
            val point = pointFrom(args.arg(1))
            val world = point?.world ?: args.checkjstring(1)
            val x = point?.x ?: args.todouble(2)
            val z = point?.z ?: args.todouble(3)
            pauseUntil { resume ->
                val arranged =
                    host.services.at(world, x, z) {
                        instance.sandbox.refill()
                        host.resuming(plugin) { resume() }
                    }
                // Nobody owns it (an unloaded world): carry on where we are rather than
                // dropping the rest of the turn on the floor.
                if (!arranged) resume()
            }
            return Lua.NONE
        }
    }

/** `pos(world, x, y, z)`: somewhere, written down once and passed around after that. */
internal fun position(): LuaValue =
    HostFunction("pos") { args ->
        positionObject(
            args.checkjstring(1),
            args.todouble(2),
            args.todouble(3),
            args.todouble(4),
        )
    }
