package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field

/**
 * `sidebar{ priority = 50, when = ..., title = ..., lines = ... }`, and the same for the rest.
 *
 * A plugin is one more profile beside the operator's, competing by the rule the config files
 * already use. The priority is asked for and never defaulted, because a hidden default is how
 * two plugins end up tied and the winner becomes whoever the file system happened to list first.
 */
internal fun claim(
    host: PluginHost,
    plugin: LoadedPlugin,
    surface: PluginSurface,
): LuaValue =
    HostFunction(surface.surfaceName) { args ->
        val options = args.arg(1)
        if (!options.istable()) {
            throw luaError("${surface.surfaceName}{ priority = ..., lines = ... } takes a table")
        }
        val priority = options.field("priority")
        if (!priority.isnumber()) {
            throw luaError(
                "${surface.surfaceName}{} needs a priority: it is what decides who draws it when " +
                    "two plugins want to, and there is no default so a tie cannot happen by accident",
            )
        }
        val applies = options.field("when").takeIf { it.isfunction() }
        val title = options.field("title")
        val lines = options.field("lines")
        val claim =
            PluginSurfaceClaim(
                pluginId = plugin.manifest.id,
                surface = surface,
                priority = priority.toint(),
                appliesTo =
                    applies?.let { fn ->
                        { player: PluginPlayerRef ->
                            host.ask(plugin, fn, player, "${surface.surfaceName} when")?.toboolean() ?: false
                        }
                    },
                render = { player ->
                    val drawnTitle = textOf(host, plugin, title, player, surface)
                    val drawnLines = linesOf(host, plugin, lines, player, surface)
                    if (drawnTitle == null && drawnLines == null) {
                        null
                    } else {
                        PluginSurfaceContent(drawnTitle.orEmpty(), drawnLines.orEmpty())
                    }
                },
            )
        val handle = host.surfaces.claim(claim)
        plugin.registrations.add("${surface.surfaceName}(priority ${claim.priority})") { handle.cancel() }
        Lua.NIL
    }

/** A string, or a function that gives one back per player. `@key` works here like anywhere. */
internal fun textOf(
    host: PluginHost,
    plugin: LoadedPlugin,
    value: LuaValue,
    player: PluginPlayerRef,
    surface: PluginSurface,
): String? =
    when {
        value.isfunction() ->
            host
                .ask(plugin, value, player, "${surface.surfaceName} title")
                ?.takeIf { it.isstring() }
                ?.let { plugin.text.resolve(it.tojstring(), Lua.NIL) }

        value.isstring() -> plugin.text.resolve(value.tojstring(), Lua.NIL)
        else -> null
    }

internal fun linesOf(
    host: PluginHost,
    plugin: LoadedPlugin,
    value: LuaValue,
    player: PluginPlayerRef,
    surface: PluginSurface,
): List<String>? {
    val table =
        when {
            value.isfunction() -> host.ask(plugin, value, player, "${surface.surfaceName} lines")
            value.istable() -> value
            else -> null
        } ?: return null
    if (!table.istable()) return null
    val out = mutableListOf<String>()
    for (index in 1..table.length()) {
        out += plugin.text.resolve((table.get(index) ?: Lua.NIL).tojstring(), Lua.NIL)
    }
    return out
}
