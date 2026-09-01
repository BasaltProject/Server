package org.basaltmc.server.plugins

import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test

/** The frozen Kotlin as the oracle for what a plugin writes before anyone reads it. */
class PluginTextOracle {
    private val folder = "build/test-data/oracle_text"

    @AfterTest
    fun cleanUp() {
        wipe(folder)
    }

    private fun wipe(path: String) {
        if (FileIO.directoryExists(path)) {
            for (name in FileIO.list(path)) wipe("$path/$name")
        }
        FileIO.delete(path)
    }

    private fun write(
        path: String,
        text: String,
    ) {
        FileIO.createDirectories(path.substringBeforeLast('/'))
        FileIO.writeText(path, text)
    }

    private fun arguments(vararg pairs: Pair<String, Any>): LuaValue {
        val table = LuaTable()
        for ((key, value) in pairs) {
            when (value) {
                is String -> table.set(key, LuaValue.valueOf(value))
                is Int -> table.set(key, LuaValue.valueOf(value))
                is Boolean -> table.set(key, LuaValue.valueOf(value))
                else -> Unit
            }
        }
        return table
    }

    @Test
    fun dump() {
        wipe(folder)
        write(
            "$folder/lang/en_us.toml",
            """
            greeting = "Hello, {name}!"
            plain = "Nothing to fill"

            [arena]
            joined = "{player} joined {arena}"
            """.trimIndent(),
        )
        write(
            "$folder/lang/es_es.toml",
            """
            greeting = "Hola, {name}!"
            solo_espanol = "Solo aqui"
            """.trimIndent(),
        )
        // Not a locale file, and one that is not readable TOML: both skipped rather than fatal.
        write("$folder/lang/notes.txt", "ignored")
        write("$folder/lang/broken.toml", "this = ")

        val text = PluginText(folder, "en_us")
        text.load()

        val args = arguments("name" to "Whiron", "player" to "Ana", "arena" to "Sky", "count" to 3, "flag" to true)
        for (
        (name, message) in
        listOf(
            "translated" to "@greeting",
            "nested" to "@arena.joined",
            "noArguments" to "@plain",
            // A key nothing translates comes back as the key.
            "missingKey" to "@nope.at.all",
            // Only Spanish has this one, and the default locale does not: any locale will do.
            "otherLocale" to "@solo_espanol",
            "literal" to "Plain text",
            "literalWithArgs" to "Hello, {name}!",
            // An argument that was not passed stays as it was written.
            "unknownArgument" to "Hello, {nobody}!",
            "several" to "{player} in {arena} with {count} and {flag}",
            // A brace that never closes takes the rest of the line with it.
            "unclosed" to "Hello, {name",
            "empty" to "",
            "onlyBraces" to "{}",
        )
        ) {
            println("ORACLE $name [${text.resolve(message, args)}]")
        }
        println("ORACLE noTable [${text.resolve("Hello, {name}!", null)}]")
        println("ORACLE notATable [${text.resolve("Hello, {name}!", LuaValue.valueOf("nope"))}]")

        // The same files with Spanish as the default: which locale is preferred has to be the one
        // asked for, not whichever sorts first.
        val spanish = PluginText(folder, "es_es")
        spanish.load()
        println("ORACLE spanish [${spanish.resolve("@greeting", args)}] fallback=[${spanish.resolve("@plain", args)}]")

        // A plugin with no lang folder at all, which is most of them.
        val bare = PluginText("$folder/nothing", "en_us")
        bare.load()
        println("ORACLE bare [${bare.resolve("@greeting", args)}] literal=[${bare.resolve("Hi", args)}]")
    }
}
