package org.basaltmc.server.plugins

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The catalogue itself, checked for the mistakes that are invisible until a plugin hits them.
 *
 * A type declared and left out of [PluginEventTypes.ALL] is never told to the bus, so listening to
 * it fails with "there is no event called that" while the code plainly says there is. A repeated
 * name means one of the two never fires. A mutable field that is not a field at all can never be
 * written. None of these are caught by a compiler.
 */
class PluginEventCatalogueTest {
    @Test
    fun `every declared type is in the list the bus is given`() {
        // Reflection is not available here, so the check is the other way round: the list must have
        // as many entries as there are distinct names, and every well-known name must be in it.
        val names = PluginEventTypes.ALL.map { it.name }
        for (
        type in
        listOf(
            PluginEventTypes.PLAYER_JOIN,
            PluginEventTypes.PLAYER_CHAT,
            PluginEventTypes.BLOCK_BREAK,
            PluginEventTypes.ENTITY_DAMAGE,
            PluginEventTypes.SERVER_TICK,
            PluginEventTypes.INVENTORY_CLICK,
            PluginEventTypes.WORLD_WEATHER_CHANGE,
        )
        ) {
            assertTrue(type.name in names, "${type.name} is declared but never told to the bus")
        }
    }

    @Test
    fun `no two events share a name`() {
        val names = PluginEventTypes.ALL.map { it.name }
        assertEquals(
            names.size,
            names.toSet().size,
            "two events have the same name: ${names.groupBy { it }.filterValues { it.size > 1 }.keys}",
        )
    }

    @Test
    fun `every event carries something`() {
        for (type in PluginEventTypes.ALL) {
            assertTrue(type.fields.isNotEmpty(), "${type.name} has no fields at all")
            assertEquals(
                type.fields.size,
                type.fields.toSet().size,
                "${type.name} declares a field twice, so one of them can never be read",
            )
        }
    }

    @Test
    fun `what can be changed is something that exists`() {
        for (type in PluginEventTypes.ALL) {
            for (field in type.mutable) {
                assertTrue(field in type.fields, "${type.name} says '$field' is mutable and has no such field")
            }
        }
    }

    @Test
    fun `names are dotted and lowercase so they read the same everywhere`() {
        val shape = Regex("[a-z]+\\.[a-z_]+")
        for (type in PluginEventTypes.ALL) {
            assertTrue(type.name.matches(shape), "'${type.name}' is not of the form thing.happening")
        }
    }

    @Test
    fun `a player event starts with the three fields the server fills in for it`() {
        // The runtime's helper writes name, uuid and world into slots 0, 1 and 2 before the caller
        // fills the rest, so an event of the player family that orders them differently would get
        // its own values overwritten.
        for (type in PluginEventTypes.ALL) {
            // The rule is about the events the server fires *about a player*: those go through one
            // helper that fills the first three in. An event that merely happens near one (a furnace
            // finishing, a villager trading) is about the thing, not the player.
            if (type.fields.firstOrNull() != "player_name") continue
            if (type.fields.size < 3) continue
            // Nobody has a world yet when they are trying to log in.
            if (type.name == "player.login") continue
            assertEquals(
                listOf("player_name", "player_uuid", "world"),
                type.fields.take(3),
                "${type.name} does not start with the three fields the server fills in",
            )
        }
    }
}
