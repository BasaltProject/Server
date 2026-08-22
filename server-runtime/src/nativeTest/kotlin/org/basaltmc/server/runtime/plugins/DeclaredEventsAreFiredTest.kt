package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.wire.io.FileIO
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * Every event this server declares has to be fired by it somewhere.
 *
 * A declared event nobody fires is worse than one that does not exist: a plugin registers for it,
 * the registration is accepted, and it waits forever for something that never comes. This caught
 * sixteen of them the first time it was run, which is why it is a test and not a habit.
 */
class DeclaredEventsAreFiredTest {
    @Test
    fun everyDeclaredEventIsFiredSomewhere() {
        val sources = collect("src/nativeMain/kotlin") + collect("../server-plugins/src/nativeMain/kotlin")
        val text = sources.joinToString("\n") { FileIO.readText(it).orEmpty() }
        val never =
            PluginEventTypes.ALL
                .map { type -> type.name to constantOf(type.name) }
                .filter { (_, constant) -> !text.contains("PluginEventTypes.$constant") }
                .map { it.first }
        assertTrue(
            never.isEmpty(),
            "declared and never fired, so a plugin listening to them would wait forever: $never",
        )
    }

    /** `player.item_held` is `PLAYER_ITEM_HELD`: the catalogue names its constants after the event. */
    private fun constantOf(name: String) = name.replace('.', '_').uppercase()

    private fun collect(root: String): List<String> {
        if (!FileIO.directoryExists(root)) return emptyList()
        val found = mutableListOf<String>()
        for (entry in FileIO.list(root)) {
            val path = "$root/$entry"
            if (FileIO.directoryExists(path)) {
                found += collect(path)
            } else if (entry.endsWith(".kt")) {
                found += path
            }
        }
        return found
    }
}
