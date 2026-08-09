package org.basaltmc.server.placeholder

import kotlin.math.abs
import kotlin.math.round

/**
 * Rounding for placeholders that resolve to a number.
 *
 * Any numeric placeholder takes a `_<digits>` suffix saying how many decimals to show, so
 * `%basalt_player_x_2%` is `21.71` where `%basalt_player_x%` is `21.70704236703265`. A raw double is
 * the honest answer to "where is the player" and an unreadable one to put on a scoreboard, and
 * neither side should have to win: the file says which it wants.
 *
 * Written out rather than delegated to a format library because Kotlin/Native's common standard
 * library has no `String.format`, and the whole need is fixed-point with a handful of digits.
 */
internal object Decimals {
    /** The most digits worth offering: past this a double is not saying anything true anyway. */
    const val MAX_DIGITS: Int = 6

    /** `x_2` to `x` and 2, or null when there is no such suffix. */
    fun split(params: String): Pair<String, Int>? {
        val separator = params.lastIndexOf('_')
        if (separator <= 0 || separator == params.length - 1) return null
        val suffix = params.substring(separator + 1)
        if (suffix.length > 1 || !suffix[0].isDigit()) return null
        return params.substring(0, separator) to suffix[0].digitToInt()
    }

    fun format(
        value: Double,
        digits: Int,
    ): String {
        if (digits <= 0) return round(value).toLong().toString()
        var scale = 1L
        repeat(digits.coerceAtMost(MAX_DIGITS)) { scale *= 10 }
        val scaled = round(abs(value) * scale).toLong()
        val sign = if (value < 0 && scaled != 0L) "-" else ""
        return "$sign${scaled / scale}.${(scaled % scale).toString().padStart(digits, '0')}"
    }
}
