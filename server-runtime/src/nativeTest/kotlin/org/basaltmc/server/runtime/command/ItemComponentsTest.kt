package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** The `[component=value]` tail of an item argument, which the client writes and vanilla accepts. */
class ItemComponentsTest {
    private fun parse(input: String): ParsedItem {
        val reader = StringReader(input)
        return ItemArgument.parse(reader, ConsoleCommandSource(), testEnvironment())
    }

    @Test
    fun aPlainItemHasNoComponents() {
        val parsed = parse("minecraft:diamond_sword")
        assertEquals("minecraft:diamond_sword", parsed.id.toString())
        assertTrue(parsed.components.keys.isEmpty())
    }

    @Test
    fun enchantmentsAreParsedIntoTheComponentTheWireWrites() {
        val parsed = parse("minecraft:diamond_sword[minecraft:enchantments={\"minecraft:sharpness\":5}]")
        val enchantments = parsed.components[PlayerInventory.ENCHANTMENTS_COMPONENT] as NbtCompound
        assertEquals(5, enchantments.getInt("minecraft:sharpness"))
    }

    @Test
    fun aComponentThatCannotBeWrittenIsRefusedByName() {
        val failure = assertFailsWith<CommandSyntaxException> { parse("minecraft:diamond_sword[custom_name='x']") }
        assertTrue("custom_name" in PlainTextSerializer.serialize(failure.component))
    }
}
