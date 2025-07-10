package org.basaltmc.server.lua

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs

/**
 * The corner of Luak's surface this server uses, without the nullability.
 *
 * Luak grew out of a Java library and says `LuaValue?` in places where the value is never absent.
 * Sorting that out once here is what keeps every call site free of the `!!` this project does not
 * allow outside tests.
 */
public object Lua {
    public val NIL: LuaValue = LuaValue.NIL
    public val NONE: Varargs = requireNotNull(LuaValue.NONE) { "luak has no NONE" }
    public val TRUE: LuaValue = LuaValue.TRUE
    public val FALSE: LuaValue = LuaValue.FALSE

    public fun of(value: String): LuaValue = LuaValue.valueOf(value)

    public fun of(value: Long): LuaValue = LuaValue.valueOf(value)

    public fun of(value: Int): LuaValue = LuaValue.valueOf(value)

    public fun of(value: Double): LuaValue = requireNotNull(LuaValue.valueOf(value)) { "luak lost a number" }

    public fun of(value: Boolean): LuaValue = if (value) TRUE else FALSE

    public fun table(): LuaTable = LuaTable()

    public fun varargs(vararg values: LuaValue): Varargs = LuaValue.varargsOf(arrayOf(*values))

    /** The value at [key], never null: a table that has nothing there answers `nil` like Lua does. */
    public fun LuaValue.field(key: String): LuaValue = this.get(key) ?: NIL

    /** The value at [index] of a varargs, never null. */
    public fun Varargs.at(index: Int): LuaValue = this.arg(index)

    /** A string argument, or null when it is absent or not one. */
    public fun Varargs.stringOrNull(index: Int): String? = if (this.isstring(index)) this.tojstring(index) else null

    /** An integer argument, or [default] when it is absent. */
    public fun Varargs.intOr(
        index: Int,
        default: Int,
    ): Int = this.optint(index, default)

    /** A function argument, or null when it is absent or not one. */
    public fun Varargs.functionOrNull(index: Int): LuaValue? = this.arg(index).takeIf { it.isfunction() }
}
