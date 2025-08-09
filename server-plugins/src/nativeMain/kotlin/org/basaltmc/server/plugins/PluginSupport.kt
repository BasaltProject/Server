package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua

internal fun handleTable(handle: PluginHandle): LuaTable {
    val table = Lua.table()
    table.set(
        "cancel",
        HostFunction("cancel") {
            handle.cancel()
            Lua.NONE
        },
    )
    return table
}

internal fun luaError(message: String): net.blueva.luak.LuaError = net.blueva.luak.LuaError(message)
