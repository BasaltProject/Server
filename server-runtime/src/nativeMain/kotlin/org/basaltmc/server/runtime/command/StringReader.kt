package org.basaltmc.server.runtime.command

import org.basaltmc.wire.text.Component

/**
 * A command line being consumed left to right, and the errors that come out of it.
 *
 * Reimplements what brigadier's own `StringReader` does, error strings included: those strings are
 * observable, the client shows them verbatim under the command box, so "Expected integer" is
 * specification, not phrasing. They are plain literals in brigadier rather than translation keys,
 * which is why they are literals here too.
 */
public class StringReader(
    public val string: String,
) {
    public var cursor: Int = 0

    public val remaining: String
        get() = string.substring(cursor)

    public val remainingLength: Int
        get() = string.length - cursor

    public fun canRead(offset: Int = 1): Boolean = cursor + offset <= string.length

    public fun peek(offset: Int = 0): Char = string[cursor + offset]

    public fun read(): Char = string[cursor++]

    public fun skip() {
        cursor++
    }

    public fun skipWhitespace() {
        while (canRead() && peek().isWhitespace()) skip()
    }

    public fun expect(symbol: Char) {
        if (!canRead() || peek() != symbol) throw syntaxError("Expected '$symbol'")
        skip()
    }

    public fun readUnquotedString(): String {
        val start = cursor
        while (canRead() && isAllowedInUnquotedString(peek())) skip()
        return string.substring(start, cursor)
    }

    /**
     * Consumes [literal] if it is what comes next, followed by a space or the end of the line.
     *
     * parity: `LiteralCommandNode.parse`, which matches text rather than a word: `>=` and `<` are
     * literals in `/execute if score` and no word reader would ever read them.
     */
    public fun readLiteral(literal: String): Boolean {
        if (!string.startsWith(literal, cursor)) return false
        val end = cursor + literal.length
        if (end < string.length && string[end] != ' ') return false
        cursor = end
        return true
    }

    /** A quoted string if the next character opens a quote, an unquoted word otherwise, as brigadier does. */
    public fun readString(): String {
        if (!canRead()) return ""
        val next = peek()
        if (next == '"' || next == '\'') {
            skip()
            return readStringUntil(next)
        }
        return readUnquotedString()
    }

    /**
     * A namespaced id: the same characters an unquoted string allows, plus `:` and `/`.
     *
     * parity: `Identifier.read`, which is why `minecraft:oak_log` is one argument rather than
     * stopping at the colon the way brigadier's own unquoted reader would.
     */
    public fun readIdentifier(): String {
        val start = cursor
        while (canRead() && isAllowedInIdentifier(peek())) skip()
        return string.substring(start, cursor)
    }

    public fun readInt(): Int {
        val start = cursor
        while (canRead() && isAllowedInNumber(peek())) skip()
        val text = string.substring(start, cursor)
        if (text.isEmpty()) throw syntaxError("Expected integer")
        return text.toIntOrNull() ?: run {
            cursor = start
            throw syntaxError("Invalid integer '$text'")
        }
    }

    public fun readDouble(): Double {
        val start = cursor
        while (canRead() && isAllowedInNumber(peek())) skip()
        val text = string.substring(start, cursor)
        if (text.isEmpty()) throw syntaxError("Expected double")
        return text.toDoubleOrNull() ?: run {
            cursor = start
            throw syntaxError("Invalid double '$text'")
        }
    }

    private fun readStringUntil(terminator: Char): String {
        val result = StringBuilder()
        var escaped = false
        while (canRead()) {
            val next = read()
            when {
                escaped ->
                    if (next == terminator || next == '\\') {
                        result.append(next)
                        escaped = false
                    } else {
                        cursor--
                        throw syntaxError("Invalid escape sequence '$next' in quoted string")
                    }
                next == '\\' -> escaped = true
                next == terminator -> return result.toString()
                else -> result.append(next)
            }
        }
        throw syntaxError("Unclosed quoted string")
    }

    public fun syntaxError(message: String): CommandSyntaxException =
        CommandSyntaxException(Component.text(message), string, cursor)

    private companion object {
        fun isAllowedInUnquotedString(c: Char): Boolean =
            c in '0'..'9' || c in 'A'..'Z' || c in 'a'..'z' || c == '_' || c == '-' || c == '.' || c == '+'

        fun isAllowedInIdentifier(c: Char): Boolean = isAllowedInUnquotedString(c) || c == ':' || c == '/'

        fun isAllowedInNumber(c: Char): Boolean =
            c in '0'..'9' || c == '-' || c == '.' || c == '+' || c == 'e' || c == 'E'
    }
}

/**
 * A command that could not be parsed or refused to run. [input] and [cursor] are what let the
 * caller print vanilla's own "context" line, the grey prefix with `<--[HERE]` marking where the
 * parser stopped; a failure that has no position (a command that parsed but then declined) leaves
 * [cursor] at -1 and gets no such line, exactly like vanilla.
 */
public class CommandSyntaxException(
    public val component: Component,
    public val input: String? = null,
    public val cursor: Int = -1,
) : Exception()
