package org.basaltmc.server.plugins

import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.wire.common.toml.Toml
import org.basaltmc.wire.common.toml.TomlValue
import org.basaltmc.wire.io.FileIO

/**
 * What a plugin writes, before it reaches anyone: `@key` looked up in its own `lang/`, and `{name}`
 * filled in from a table.
 *
 * Both are the same idea: a plugin writes what it means and never assembles a string, so a
 * translated line and a line with a number in it are written the same way.
 */
internal class PluginText(
    private val folder: String,
    private val defaultLocale: String,
) {
    private val locales = mutableMapOf<String, Map<String, String>>()

    /** Reads every `lang/<locale>.toml` the plugin has. Missing is not an error: most have none. */
    fun load() {
        val directory = "$folder/lang"
        if (!FileIO.directoryExists(directory)) return
        for (entry in FileIO.list(directory)) {
            if (!entry.endsWith(".toml")) continue
            val text = FileIO.readText("$directory/$entry") ?: continue
            val table =
                runCatching { Toml.parse(text) }.getOrNull() ?: continue
            locales[entry.removeSuffix(".toml").lowercase()] = flatten(table)
        }
    }

    /**
     * Resolves one message: `@key` becomes its translation, `{name}` becomes its argument.
     *
     * A key with no translation comes back as itself rather than as an empty line, because a
     * missing string should look like a missing string to whoever has to fix it.
     */
    fun resolve(
        message: String,
        arguments: LuaValue?,
    ): String {
        val text =
            if (message.startsWith('@')) {
                val key = message.substring(1)
                translation(key) ?: key
            } else {
                message
            }
        return substitute(text, arguments)
    }

    private fun translation(key: String): String? =
        locales[defaultLocale]?.get(key) ?: locales.values.firstNotNullOfOrNull { it[key] }

    private fun substitute(
        text: String,
        arguments: LuaValue?,
    ): String {
        if ('{' !in text || arguments == null || !arguments.istable()) return text
        val out = StringBuilder(text.length)
        var index = 0
        while (index < text.length) {
            val char = text[index]
            if (char != '{') {
                out.append(char)
                index++
                continue
            }
            val close = text.indexOf('}', index)
            if (close < 0) {
                out.append(text.substring(index))
                break
            }
            val name = text.substring(index + 1, close)
            val value = arguments.get(name)
            // An argument that was not passed stays as it was written: `{name}` on the screen is a
            // bug report, an empty gap is a mystery.
            out.append(if (value == null || value.isnil()) "{$name}" else value.tojstring())
            index = close + 1
        }
        return out.toString()
    }

    private fun flatten(
        table: TomlValue.Table,
        prefix: String = "",
    ): Map<String, String> {
        val out = mutableMapOf<String, String>()
        for (key in table.keys()) {
            when (val value = table[key]) {
                is TomlValue.Str -> out["$prefix$key"] = value.value
                is TomlValue.Table -> out += flatten(value, "$prefix$key.")
                else -> Unit
            }
        }
        return out
    }

    companion object {
        /** `p:send(message)` and `p:send(message, { ... })`, with or without the self argument. */
        fun messageAndArguments(args: Varargs): Pair<Int, Int> {
            val first = firstArgument(args)
            return first to first + 1
        }
    }
}
