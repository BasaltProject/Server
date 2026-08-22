package org.basaltmc.server.runtime.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Every packet this server sends or dispatches has a name a plugin can use.
 *
 * The table is derived rather than invented, so the way it goes wrong is by falling behind: somebody
 * adds a packet and it shows up to plugins as a number. This walks the sources the same way the
 * table was built, so that fails the build instead.
 */
class PacketNamesAreCompleteTest {
    @Test
    fun everyPacketTheServerUsesHasAName() {
        val text = sources().joinToString("\n") { FileIO.readText(it).orEmpty() }
        val dispatched = Regex("""(\w+Packet)\.(?:ID|SERVERBOUND_ID)\s*->""").findAll(text).map { it.groupValues[1] }
        val sent = Regex("""send\w*\((\w+Packet)\.(?:ID|CLIENTBOUND_ID)""").findAll(text).map { it.groupValues[1] }
        val used = (dispatched + sent).toSet()
        val named = PacketNames.all().map { it.substringAfter('.') }.toSet()

        val missing = used.filterNot { named.contains(snake(it)) }.sorted()
        assertEquals(emptyList(), missing, "these packets are used but have no name for plugins")
    }

    @Test
    fun namesLookLikeTheProtocolsOwn() {
        for (name in PacketNames.all()) {
            assertTrue(
                name.startsWith("clientbound.") || name.startsWith("serverbound."),
                "'$name' does not say which way it goes",
            )
            val bare = name.substringAfter('.')
            assertTrue(bare.matches(Regex("[a-z0-9_]+")), "'$bare' is not the protocol's own spelling")
        }
    }

    private fun snake(className: String): String {
        var name = className.removeSuffix("Packet")
        for (prefix in listOf("Serverbound", "Clientbound")) name = name.removePrefix(prefix)
        return name
            .replace(Regex("(?<!^)(?=[A-Z])"), "_")
            .lowercase()
            .replace(Regex("_+"), "_")
    }

    private fun sources(): List<String> {
        val out = mutableListOf<String>()

        fun walk(path: String) {
            if (FileIO.directoryExists(path)) {
                for (entry in FileIO.list(path)) walk("$path/$entry")
            } else if (path.endsWith(".kt")) {
                out += path
            }
        }
        walk("src/nativeMain/kotlin")
        return out
    }
}
