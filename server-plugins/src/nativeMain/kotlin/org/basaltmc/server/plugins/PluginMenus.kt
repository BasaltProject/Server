package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field

/** `menu{ title = ..., rows = 3 }`: a window with buttons in it, and no slot arithmetic. */
internal fun menu(
    host: PluginHost,
    plugin: LoadedPlugin,
): LuaValue =
    HostFunction("menu") { args ->
        val options = args.arg(1)
        if (!options.istable()) throw luaError("menu{ title = ..., rows = 3 } takes a table")
        val rows = options.field("rows").takeIf { it.isnumber() }?.toint() ?: 3
        if (rows !in 1..PluginMenus.MAX_ROWS) {
            throw luaError("a menu has between 1 and ${PluginMenus.MAX_ROWS} rows, not $rows")
        }
        val title =
            options
                .field("title")
                .takeIf { it.isstring() }
                ?.tojstring()
                .orEmpty()
        val built =
            LuaMenu(
                host.services.menus,
                host.services.players,
                plugin.text.resolve(title, Lua.NIL),
                rows,
            ) { handler, player, slot ->
                host.clicked(plugin, handler, player, slot)
            }
        plugin.registrations.add("menu($title)") { built.closeAll() }
        built.table
    }
