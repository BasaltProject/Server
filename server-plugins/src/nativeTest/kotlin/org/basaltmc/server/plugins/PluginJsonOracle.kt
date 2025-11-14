package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for what a plugin saves and loads as JSON.
 *
 * Two things here are deliberately compared by content and not by text. The order an object's keys
 * come out in is whatever the interpreter's own `next` gives, and the two interpreters do not agree;
 * and the length of a Lua array with a hole in it is a border the language leaves unspecified. What
 * a plugin can rely on is the values, so that is what is pinned.
 */
class PluginJsonOracle {
    private fun table(vararg pairs: Pair<String, LuaValue>): LuaTable =
        LuaTable().also { for ((key, value) in pairs) it.set(key, value) }

    private fun array(vararg values: LuaValue): LuaTable =
        LuaTable().also { for ((index, value) in values.withIndex()) it.set(index + 1, value) }

    /** Every value in the tree, keys sorted, so two interpreters can be compared at all. */
    private fun describe(value: LuaValue): String =
        when {
            value.isnil() -> "nil"
            value.isboolean() -> value.toboolean().toString()
            value.isint() -> "i" + value.tolong()
            value.isnumber() -> "n" + value.todouble()
            value.istable() -> describeTable(value)
            else -> "s" + value.tojstring()
        }

    private fun describeTable(table: LuaValue): String {
        val entries = mutableListOf<String>()
        var key: LuaValue = LuaValue.NIL
        while (true) {
            val pair = table.next(key) ?: break
            val nextKey = pair.arg1()
            if (nextKey.isnil()) break
            entries += describe(nextKey) + "=" + describe(pair.arg(2))
            key = nextKey
        }
        return "{" + entries.sorted().joinToString(",") + "}"
    }

    @Test
    fun dump() {
        val document =
            table(
                "name" to LuaValue.valueOf("Arena"),
                "players" to LuaValue.valueOf(24),
                "rate" to LuaValue.valueOf(1.5),
                "open" to LuaValue.TRUE,
                "closed" to LuaValue.FALSE,
                "worlds" to array(LuaValue.valueOf("arena"), LuaValue.valueOf("lobby")),
                "nested" to table("depth" to LuaValue.valueOf(2)),
                "empty" to LuaTable(),
            )
        // What was written, read back and described: the text differs in key order, the content
        // does not.
        println("ORACLE writeContent " + describe(PluginJson.parse(PluginJson.write(document))))

        // An array's text is defined, because its order is.
        println(
            "ORACLE writeArray " +
                PluginJson.write(array(LuaValue.valueOf(1), LuaValue.valueOf("two"), LuaValue.TRUE)),
        )
        println(
            "ORACLE writeScalars " + PluginJson.write(LuaValue.valueOf(7)) + "/" +
                PluginJson.write(LuaValue.valueOf(7.5)) + "/" + PluginJson.write(LuaValue.TRUE) + "/" +
                PluginJson.write(LuaValue.NIL),
        )
        // Everything the writer has to escape, in one string, including a bare control character.
        val tricky = "a\"b\\c" + Char(10) + "d" + Char(13) + "e" + Char(9) + "f" + Char(1) + "g"
        println("ORACLE escapes " + PluginJson.write(LuaValue.valueOf(tricky)))
        println("ORACLE writeNestedArray " + PluginJson.write(array(array(LuaValue.valueOf(3), LuaValue.valueOf(4)))))
        // Past what a double can hold exactly, which is the only place writing an integer as a
        // number is visible.
        println("ORACLE writeBigInteger " + PluginJson.write(LuaValue.valueOf(9007199254740993L)))

        val documents =
            listOf(
                "object" to """{"name":"Arena","players":24,"rate":1.5,"open":true,"missing":null}""",
                "array" to """[1,"two",true,[3,4],{"k":"v"}]""",
                "nested" to """{"a":{"b":{"c":[1,2,3]}}}""",
                "escapes" to """{"text":"a\"b\\c\nd\re\tf\u0041g"}""",
                "spaced" to "  {  \"a\" :  1 ,  \"b\" : [ 2 , 3 ]  }  ",
                "empty" to "{}",
                "emptyArray" to "[]",
                "numbers" to """{"whole":7,"negative":-12,"real":1.25,"exponent":2e3,"zero":0}""",
                // Not a document at all: an empty table rather than a failure.
                "scalar" to "42",
                "nothing" to "",
                // Truncated, which a saved file can be after a crash.
                "truncated" to """{"a":1,"b":""",
                // A literal that starts like one and is not: the reader gives up rather than
                // reading past it forever.
                "badLiteral" to """{"a":tXue,"b":2}""",
            )
        for ((name, text) in documents) {
            println("ORACLE parse" + name + " " + describe(PluginJson.parse(text)))
        }

        // The round trip, which is what a plugin saving and loading its own data does.
        println("ORACLE roundTrip " + describe(PluginJson.parse(PluginJson.write(document))))
    }
}
