package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs
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
    fun theOtherComponentsWithACodecAreAcceptedToo() {
        val parsed = parse("""minecraft:diamond_sword[custom_name='{"text":"Sting"}',damage=42,unbreakable={}]""")
        assertEquals(
            setOf("minecraft:custom_name", "minecraft:damage", "minecraft:unbreakable"),
            parsed.components.keys,
        )
        assertEquals(NbtInt(42), parsed.components["minecraft:damage"])
    }

    @Test
    fun aComponentWithNoNetworkCodecIsStoredAndReported() {
        // Accepted, stored and persisted; what it cannot do is reach a client, and the command says so.
        val parsed = parse("minecraft:diamond_sword[attribute_modifiers={}]")
        assertEquals(setOf("minecraft:attribute_modifiers"), parsed.components.keys)
        assertEquals(listOf("minecraft:attribute_modifiers"), ItemComponentCodecs.notDrawn(parsed.components))
    }

    @Test
    fun theComponentsWithAPayloadCodecDoReachTheClient() {
        val parsed = parse("minecraft:diamond_pickaxe[enchantable={value:10},food={nutrition:4,saturation:0.3}]")
        assertTrue(ItemComponentCodecs.notDrawn(parsed.components).isEmpty())
        assertEquals(2, ItemComponentCodecs.toWire(parsed.components) { null }.size)
    }

    @Test
    fun aComponentVanillaNeverSyncsTravelsAsNbt() {
        // parity: no `networkSynchronized` means `fromCodecWithRegistries`, which puts the
        // persistent value on the wire as NBT.
        val parsed = parse("""minecraft:diamond_sword[lock={}]""")
        assertTrue(ItemComponentCodecs.notDrawn(parsed.components).isEmpty())
    }

    @Test
    fun aNameThatIsNoComponentAtAllSaysThatInstead() {
        val failure = assertFailsWith<CommandSyntaxException> { parse("minecraft:diamond_sword[nonsense=1]") }
        assertTrue("Unknown item component" in PlainTextSerializer.serialize(failure.component))
    }

    @Test
    fun everyStoredComponentSurvivesTheTripToTheWireAndBack() {
        val stored = parse("""minecraft:diamond_sword[damage=7,rarity="epic",dyed_color=255]""").components
        val wire = ItemComponentCodecs.toWire(stored) { null }
        assertEquals(3, wire.size)
        assertIs<ItemComponent.Number>(wire.first { it.id == ItemComponent.DAMAGE })
        assertEquals(stored, ItemComponentCodecs.toStored(wire) { null })
    }

    @Test
    fun loreKeepsItsLinesThroughTheWire() {
        val stored = parse("""minecraft:diamond_sword[lore=['{"text":"first"}','{"text":"second"}']]""").components
        val lore = assertIs<ItemComponent.Lore>(ItemComponentCodecs.toWire(stored) { null }.single())
        assertEquals(2, lore.lines.size)
    }

    @Test
    fun anEnchantmentThatTheRegistryCannotResolveIsLeftOffRatherThanGuessed() {
        val stored = parse("minecraft:diamond_sword[minecraft:enchantments={\"minecraft:sharpness\":5}]").components
        assertTrue(ItemComponentCodecs.toWire(stored) { null }.isEmpty())
        val resolved = ItemComponentCodecs.toWire(stored) { if (it == Identifier.vanilla("sharpness")) 12 else null }
        assertEquals(ItemComponent.Enchantments(mapOf(12 to 5)), resolved.single())
    }
}
