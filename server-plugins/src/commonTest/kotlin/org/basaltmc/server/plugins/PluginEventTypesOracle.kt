package org.basaltmc.server.plugins

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the whole table of events this server fires. */
class PluginEventTypesOracle {
    private fun checksum(text: String): Long {
        var checksum = -3750763034362895579L
        for (char in text) checksum = (checksum xor char.code.toLong()) * 1099511628211L
        return checksum
    }

    @Test
    fun dump() {
        val lines =
            PluginEventTypes.ALL.map {
                "${it.name}|${it.cancellable}|${it.fields.joinToString(",")}|${it.mutable.sorted().joinToString(",")}"
            }
        println("ORACLE count ${lines.size} names=${PluginEventTypes.ALL.map { it.name }.toSet().size}")
        println("ORACLE checksum ${checksum(lines.joinToString(";"))}")
        // The ends of the list and a handful in the middle, spelled out: a checksum says something
        // changed, these say what it should have been.
        for (index in listOf(0, 1, 2, 17, 34, 51, lines.size - 2, lines.size - 1)) {
            println("ORACLE at$index ${lines[index]}")
        }
        println("ORACLE cancellable ${PluginEventTypes.ALL.count { it.cancellable }}")
        println(
            "ORACLE withMutable ${PluginEventTypes.ALL.filter {
                it.mutable.isNotEmpty()
            }.map { "${it.name}=${it.mutable.sorted()}" }}",
        )
        println("ORACLE lifecycle ${PluginEventTypes.PLUGIN_ENABLE}/${PluginEventTypes.PLUGIN_DISABLE}")
    }
}
