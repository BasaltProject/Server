package org.basaltmc.common.placeholder

/**
 * Replacing `%name%` in operator-written text.
 *
 * A name may not contain whitespace, which is what keeps ordinary prose safe: "50% off, and 20%
 * more" has no placeholder in it, because neither run between two per cent signs is a single word.
 * `%%` is a literal per cent sign for the cases that rule does not cover.
 *
 * A name nobody can answer is left exactly as written by default: text that still says
 * `%basalt_player_name%` tells the operator what went wrong, where text that silently lost a word
 * does not. A caller that knows better decides otherwise from [onUnresolved], because "leave the
 * debug text in" is the right answer for a config file being checked and the wrong one for a line
 * that strangers read.
 */
public object PlaceholderText {
    public fun substitute(
        text: String,
        registry: PlaceholderRegistry,
        context: PlaceholderContext = PlaceholderContext.EMPTY,
        /** What to put there, or null to keep the placeholder as written. Called only on failure. */
        onUnresolved: (name: String, value: PlaceholderValue) -> String? = { _, _ -> null },
    ): String {
        if ('%' !in text) return text
        val result = StringBuilder(text.length)
        var index = 0
        while (index < text.length) {
            val char = text[index]
            if (char != '%') {
                result.append(char)
                index++
                continue
            }
            if (index + 1 < text.length && text[index + 1] == '%') {
                result.append('%')
                index += 2
                continue
            }
            val close = closingIndex(text, index)
            if (close < 0) {
                result.append(char)
                index++
                continue
            }
            val name = text.substring(index + 1, close)
            when (val value = registry.resolve(name, context)) {
                is PlaceholderValue.Text -> result.append(value.value)
                else -> {
                    val replacement = onUnresolved(name, value)
                    if (replacement == null) result.append(text, index, close + 1) else result.append(replacement)
                }
            }
            index = close + 1
        }
        return result.toString()
    }

    /** Every placeholder name in [text], for working out up front what a piece of text will need. */
    public fun namesIn(text: String): List<String> {
        val names = mutableListOf<String>()
        var index = 0
        while (index < text.length) {
            if (text[index] != '%') {
                index++
                continue
            }
            if (index + 1 < text.length && text[index + 1] == '%') {
                index += 2
                continue
            }
            val close = closingIndex(text, index)
            if (close < 0) {
                index++
                continue
            }
            names += text.substring(index + 1, close)
            index = close + 1
        }
        return names
    }

    /** The closing `%` of the placeholder opening at [start], or -1 if this is not one. */
    private fun closingIndex(
        text: String,
        start: Int,
    ): Int {
        var index = start + 1
        while (index < text.length) {
            val char = text[index]
            if (char.isWhitespace()) return -1
            if (char == '%') return if (index == start + 1) -1 else index
            index++
        }
        return -1
    }
}

/**
 * Case-insensitive glob where `*` stands for any run of characters.
 *
 * Lives here because both the `matches` operator in a condition and any caller comparing a host or
 * an address needs the same rule, and two implementations of "what does `*` mean" is one too many.
 */
public fun globMatches(
    pattern: String,
    value: String,
): Boolean {
    val parts = pattern.lowercase().split("*")
    val text = value.lowercase()
    if (parts.size == 1) return text == parts[0]

    if (!text.startsWith(parts.first())) return false
    if (!text.endsWith(parts.last())) return false
    var index = parts.first().length
    for (part in parts.subList(1, parts.size - 1)) {
        if (part.isEmpty()) continue
        val found = text.indexOf(part, index)
        if (found < 0) return false
        index = found + part.length
    }
    // The trailing literal has to fit after everything already matched, or "a*bc" would accept "abc"
    // twice over by counting the same characters as both.
    return index <= text.length - parts.last().length
}
