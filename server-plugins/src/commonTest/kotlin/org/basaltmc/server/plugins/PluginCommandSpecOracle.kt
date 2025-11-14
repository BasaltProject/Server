package org.basaltmc.server.plugins

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the one-line command spec a plugin writes. */
class PluginCommandSpecOracle {
    private val specs =
        listOf(
            "arena",
            "arena create <name: word>",
            "arena create <name: word> [size: int 8..64]",
            // Several literals and every type, in one line.
            "shop admin set <player: word> <amount: int> <rate: float> <silent: bool> <reason: text>",
            // No type at all, which is a word.
            "kick <player>",
            // Extra spaces everywhere the tokenizer has to cope with.
            "  arena   create   < name :  word >   [ size : int  8..64 ]  ",
            // A range with only one end, and one whose ends are not numbers.
            "give <amount: int 1..>",
            "give <amount: int ..99>",
            "give <amount: int a..b>",
            // A number with something stuck to it is not a number, not a number and some rubbish.
            "give <amount: int 8x..64>",
            // No space between the name and the first argument, which the tokenizer has to split.
            "arena<name: word>",
            // Two optionals in a row, which is allowed.
            "warp <name: word> [x: int] [y: int]",
        )

    private val refused =
        listOf(
            "",
            "   ",
            "<name: word>",
            "arena <name: word> create",
            "arena [size: int] <name: word>",
            "arena <name: word",
            "arena [size: int>",
            "arena <: word>",
            "arena <name: player>",
        )

    @Test
    fun dump() {
        for (spec in specs) {
            val parsed = PluginCommandSpec.parse(spec, permission = "basalt.arena")
            println(
                "ORACLE spec[$spec] name=${parsed.name} literals=${parsed.literals}" +
                    " args=${parsed.arguments.map {
                        "${it.name}/${it.type.spelling}/${it.optional}/${it.min}/${it.max}"
                    }}" +
                    " usage=${parsed.usage} permission=${parsed.permission}",
            )
        }
        for (spec in refused) {
            val message =
                try {
                    PluginCommandSpec.parse(spec).usage
                    "accepted"
                } catch (failure: ManifestException) {
                    failure.message ?: "unknown"
                }
            println("ORACLE refused[$spec] ${message.replace(' ', '_')}")
        }
        println(
            "ORACLE types ${PluginArgumentType.entries.map { it.spelling }}" +
                " of=${PluginArgumentType.of("int")} unknown=${PluginArgumentType.of("player")}",
        )
    }
}
