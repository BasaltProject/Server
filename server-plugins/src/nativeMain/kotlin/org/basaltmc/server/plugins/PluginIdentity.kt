package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.wire.common.toml.TomlValue
import org.basaltmc.wire.io.FileIO

internal fun describe(
    plugin: LoadedPlugin,
    instance: PluginInstance,
    lanes: Int,
): LuaTable {
    val table = Lua.table()
    table.set("id", plugin.manifest.id)
    table.set("name", plugin.manifest.name)
    table.set("version", plugin.manifest.version)
    // A plugin that keeps state in a module-level variable has one of those per lane, so it is
    // told which one it is rather than having to find out the hard way.
    table.set("lane", instance.lane)
    table.set("lanes", lanes)
    return table
}

internal fun logTable(plugin: LoadedPlugin): LuaTable {
    val table = Lua.table()
    table.set(
        "info",
        HostFunction("log.info") { args ->
            plugin.log.info { args.tojstring(1) }
            Lua.NONE
        },
    )
    table.set(
        "warn",
        HostFunction("log.warn") { args ->
            plugin.log.warn { args.tojstring(1) }
            Lua.NONE
        },
    )
    table.set(
        "error",
        HostFunction("log.error") { args ->
            plugin.log.error { args.tojstring(1) }
            Lua.NONE
        },
    )
    return table
}

/**
 * `files.read`, `files.list` and `files.exists`, over the plugin's own folder.
 *
 * Paths are relative to `plugins/<id>/`, which is where the package's `resources/` were put, so
 * what a plugin asks for is what the operator sees. Anything the package kept to itself
 * (`[resources] private`) is still readable: the folder is looked at first and the package
 * after it.
 */
internal fun fileTable(plugin: LoadedPlugin): LuaTable {
    val table = Lua.table()
    table.set(
        "read",
        HostFunction("files.read") { args ->
            val path = args.checkjstring(1)
            val text =
                FileIO.readText("${plugin.folder}/$path")
                    ?: plugin.pkg.readText(PackageLayout.RESOURCES + path)
            text?.let { Lua.of(it) } ?: Lua.NIL
        },
    )
    table.set(
        "exists",
        HostFunction("files.exists") { args ->
            val path = args.checkjstring(1)
            Lua.of(FileIO.exists("${plugin.folder}/$path") || plugin.pkg.exists(PackageLayout.RESOURCES + path))
        },
    )
    table.set(
        "list",
        HostFunction("files.list") { args ->
            val prefix = if (args.narg() >= 1) args.checkjstring(1) else ""
            val found =
                plugin.files.keys
                    .filter { it.startsWith(prefix) }
                    .sorted()
            val out = Lua.table()
            for ((index, name) in found.withIndex()) out.set(index + 1, Lua.of(name))
            out
        },
    )
    table.set(
        "write",
        HostFunction("files.write") { args ->
            val path = args.checkjstring(1)
            val text = args.checkjstring(2)
            val live = "${plugin.folder}/$path"
            FileIO.createDirectories(live.substringBeforeLast('/'))
            FileIO.writeText(live, text)
            Lua.NIL
        },
    )
    return table
}

/** The TOML tree as Lua tables. Numbers stay integers where they were written as integers. */
internal fun tableOf(table: TomlValue.Table): LuaTable {
    val out = Lua.table()
    for (key in table.keys()) {
        val value = table[key] ?: continue
        val converted = valueOf(value)
        out.set(key, converted)
        // TOML is written kebab-case here and in the server's own files, and Lua cannot spell a
        // dash in `config.starting-balance`, so the same key answers to an underscore too.
        if ('-' in key) out.set(key.replace('-', '_'), converted)
    }
    return out
}

internal fun valueOf(value: TomlValue): LuaValue =
    when (value) {
        is TomlValue.Str -> Lua.of(value.value)
        is TomlValue.Int64 -> Lua.of(value.value)
        is TomlValue.Real -> Lua.of(value.value)
        is TomlValue.Bool -> Lua.of(value.value)
        is TomlValue.Arr -> {
            val list = Lua.table()
            for ((index, element) in value.values.withIndex()) list.set(index + 1, valueOf(element))
            list
        }

        is TomlValue.Table -> tableOf(value)
    }
