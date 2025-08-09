package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.wire.io.FileIO

/**
 * `store`: what the plugin keeps that is not about one player, and the queries over what is.
 *
 * `store.record = ...` is the plugin's own key-value, kept under the empty uuid so it lives in
 * the same table with the same isolation; `store.top("kills", 10)` is the query that made this
 * worth being a database rather than a file.
 */
internal fun storeTable(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaTable {
    val table = Lua.table()
    val metatable = Lua.table()
    val id = plugin.manifest.id
    metatable.set(
        "__index",
        HostFunction("store.index") { args ->
            val key = args.arg(2).tojstring()
            if (key == "top") return@HostFunction topFunction(host, id)
            val stored = host.services.storage.playerValue(id, SERVER_SCOPE, key) ?: return@HostFunction Lua.NIL
            stored.toLongOrNull()?.let { return@HostFunction Lua.of(it) }
            stored.toDoubleOrNull()?.let { return@HostFunction Lua.of(it) }
            when (stored) {
                "true" -> Lua.TRUE
                "false" -> Lua.FALSE
                else -> Lua.of(stored)
            }
        },
    )
    metatable.set(
        "__newindex",
        HostFunction("store.newindex") { args ->
            val key = args.arg(2).tojstring()
            val value = args.arg(3)
            host.services.storage.setPlayerValue(
                id,
                SERVER_SCOPE,
                key,
                if (value.isnil()) null else value.tojstring(),
            )
            Lua.NONE
        },
    )
    table.setmetatable(metatable)
    return table
}

/** `store.top("kills", 10)`: highest first, as `{ { name = ..., value = ... }, ... }`. */
internal fun topFunction(
    host: PluginHost,
    pluginId: String,
): LuaValue =
    HostFunction("store.top") { args ->
        val first = firstArgument(args)
        val key = args.checkjstring(first)
        val limit = args.optint(first + 1, 10)
        val out = Lua.table()
        for ((index, row) in host.services.storage
            .top(pluginId, key, limit)
            .withIndex()) {
            val entry = Lua.table()
            val player = host.services.players.byUuid(row.first)
            entry.set("uuid", row.first)
            entry.set("name", player?.name ?: row.first)
            entry.set("value", row.second.toLongOrNull()?.let { Lua.of(it) } ?: Lua.of(row.second))
            out.set(index + 1, entry)
        }
        out
    }

/**
 * `data.json("arenas")`: a table kept in a file of the plugin's own folder.
 *
 * For what is not about a player. It is read when asked for and written by `data.save`, atomically,
 * so a crash leaves the old file rather than half of a new one.
 */
internal fun dataTable(plugin: LoadedPlugin): LuaTable {
    val table = Lua.table()
    val loaded = mutableMapOf<String, LuaTable>()
    table.set(
        "json",
        HostFunction("data.json") { args ->
            val name = args.checkjstring(1)
            loaded.getOrPut(name) {
                val path = jsonPathOf(plugin, name)
                val text = FileIO.readText(path)
                if (text == null) Lua.table() else PluginJson.parse(text)
            }
        },
    )
    table.set(
        "save",
        HostFunction("data.save") { args ->
            val name = args.checkjstring(1)
            val value = loaded[name] ?: return@HostFunction Lua.of(false)
            val path = jsonPathOf(plugin, name)
            FileIO.createDirectories(path.substringBeforeLast('/'))
            // Serialised before the file is touched: a plugin whose table cannot be written
            // keeps the one it had rather than losing both.
            val text = PluginJson.write(value)
            FileIO.writeText(path, text)
            Lua.of(true)
        },
    )
    return table
}

/** `arenas` is `data/arenas.json` unless the plugin gave a path of its own. */
internal fun jsonPathOf(
    plugin: LoadedPlugin,
    name: String,
): String =
    if (name.contains('/') || name.endsWith(".json")) {
        "${plugin.folder}/$name"
    } else {
        "${plugin.folder}/data/$name.json"
    }

/** The uuid a plugin's own data is kept under: not a player, and no player can have it. */
internal const val SERVER_SCOPE: String = "server"
