package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import org.basaltmc.server.lua.Lua

/**
 * JSON, as much of it as a Lua table is.
 *
 * Written here rather than pulled in because the mapping is the whole problem: a Lua table is an
 * array when its keys are 1..n and an object otherwise, and no general-purpose JSON library knows
 * that. Numbers keep their kind (7 stays 7, not 7.0), which is what makes a saved file readable by
 * a person.
 */
internal object PluginJson {
    fun write(value: LuaValue): String {
        val out = StringBuilder()
        writeValue(value, out)
        return out.toString()
    }

    fun parse(text: String): LuaTable {
        val reader = Reader(text)
        reader.skipSpace()
        val value = reader.value()
        return value as? LuaTable ?: Lua.table()
    }

    private fun writeValue(
        value: LuaValue,
        out: StringBuilder,
    ) {
        when {
            value.isnil() -> out.append("null")
            value.isboolean() -> out.append(if (value.toboolean()) "true" else "false")
            value.isint() -> out.append(value.tolong())
            value.isnumber() -> out.append(value.todouble())
            value.isstring() -> writeString(value.tojstring(), out)
            value.istable() -> writeTable(value, out)
            else -> writeString(value.tojstring(), out)
        }
    }

    private fun writeTable(
        table: LuaValue,
        out: StringBuilder,
    ) {
        val length = table.length()
        // 1..n and nothing else is an array; anything else is an object, which is the one place a
        // Lua table and JSON disagree about what they are.
        if (length > 0 && table.get(Lua.of(length + 1)).isnil()) {
            out.append('[')
            for (index in 1..length) {
                if (index > 1) out.append(',')
                writeValue(table.get(index) ?: Lua.NIL, out)
            }
            out.append(']')
            return
        }
        out.append('{')
        var first = true
        var key: LuaValue = Lua.NIL
        while (true) {
            val pair = table.next(key) ?: break
            val nextKey = pair.arg1()
            if (nextKey.isnil()) break
            if (!first) out.append(',')
            first = false
            writeString(nextKey.tojstring(), out)
            out.append(':')
            writeValue(pair.arg(2), out)
            key = nextKey
        }
        out.append('}')
    }

    private fun writeString(
        value: String,
        out: StringBuilder,
    ) {
        out.append('"')
        for (char in value) {
            when (char) {
                '"' -> out.append("\\\"")
                '\\' -> out.append("\\\\")
                '\n' -> out.append("\\n")
                '\r' -> out.append("\\r")
                '\t' -> out.append("\\t")
                else ->
                    if (char.code < 0x20) {
                        out.append("\\u").append(char.code.toString(16).padStart(4, '0'))
                    } else {
                        out.append(char)
                    }
            }
        }
        out.append('"')
    }

    private class Reader(
        private val text: String,
    ) {
        private var index = 0

        fun skipSpace() {
            while (index < text.length && text[index].isWhitespace()) index++
        }

        fun value(): LuaValue {
            skipSpace()
            if (index >= text.length) return Lua.NIL
            return when (text[index]) {
                '{' -> obj()
                '[' -> array()
                '"' -> Lua.of(string())
                't' -> literal("true", Lua.TRUE)
                'f' -> literal("false", Lua.FALSE)
                'n' -> literal("null", Lua.NIL)
                else -> number()
            }
        }

        private fun literal(
            word: String,
            value: LuaValue,
        ): LuaValue {
            if (text.startsWith(word, index)) index += word.length else index = text.length
            return value
        }

        private fun obj(): LuaValue {
            val table = Lua.table()
            index++
            while (true) {
                skipSpace()
                if (index >= text.length || text[index] == '}') {
                    index++
                    return table
                }
                val key = string()
                skipSpace()
                if (index < text.length && text[index] == ':') index++
                table.set(key, value())
                skipSpace()
                if (index < text.length && text[index] == ',') index++
            }
        }

        private fun array(): LuaValue {
            val table = Lua.table()
            index++
            var position = 1
            while (true) {
                skipSpace()
                if (index >= text.length || text[index] == ']') {
                    index++
                    return table
                }
                table.set(position++, value())
                skipSpace()
                if (index < text.length && text[index] == ',') index++
            }
        }

        private fun string(): String {
            val out = StringBuilder()
            if (index < text.length && text[index] == '"') index++
            while (index < text.length && text[index] != '"') {
                val char = text[index]
                if (char == '\\' && index + 1 < text.length) {
                    index++
                    when (val escaped = text[index]) {
                        'n' -> out.append('\n')
                        'r' -> out.append('\r')
                        't' -> out.append('\t')
                        'u' -> {
                            val code = text.substring(index + 1, minOf(index + 5, text.length)).toIntOrNull(16)
                            if (code != null) out.append(code.toChar())
                            index += 4
                        }

                        else -> out.append(escaped)
                    }
                } else {
                    out.append(char)
                }
                index++
            }
            index++
            return out.toString()
        }

        private fun number(): LuaValue {
            val start = index
            while (index < text.length && (text[index].isDigit() || text[index] in "-+.eE")) index++
            val raw = text.substring(start, index)
            raw.toLongOrNull()?.let { return Lua.of(it) }
            return raw.toDoubleOrNull()?.let { Lua.of(it) } ?: Lua.NIL
        }
    }
}
