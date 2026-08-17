package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private const val MINING_EFFICIENCY = 24

/** The efficiency definition in the shape the jar ships it. */
private fun efficiencyDefinition(id: Identifier): NbtCompound? {
    if (id.path != "efficiency") return null
    return NbtCompound().apply {
        this["slots"] = NbtList(NbtType.STRING, listOf(NbtString("mainhand")))
        this["effects"] =
            NbtCompound().apply {
                this["minecraft:attributes"] =
                    NbtList(
                        NbtType.COMPOUND,
                        listOf(
                            NbtCompound().apply {
                                putString("attribute", "minecraft:mining_efficiency")
                                putString("id", "minecraft:enchantment.efficiency")
                                putString("operation", "add_value")
                                this["amount"] =
                                    NbtCompound().apply {
                                        putString("type", "minecraft:levels_squared")
                                        putDouble("added", 1.0)
                                    }
                            },
                        ),
                    )
            }
    }
}

class EnchantmentAttributesTest {
    private fun pickaxe(level: Int): StoredItemStack =
        StoredItemStack(
            id = "minecraft:diamond_pickaxe",
            count = 1,
            components =
                NbtCompound().apply {
                    this[PlayerInventory.ENCHANTMENTS_COMPONENT] =
                        NbtCompound().apply { this["minecraft:efficiency"] = NbtInt(level) }
                },
        )

    /** parity: `levels_squared` with one added, which is what makes efficiency 5 worth 26. */
    @Test
    fun efficiencyBecomesTheMiningModifierTheClientReads() {
        val snapshots = EnchantmentAttributes.snapshotsFor(pickaxe(5), ::efficiencyDefinition)

        val mining = snapshots.single { it.attribute == MINING_EFFICIENCY }
        assertEquals(1, mining.modifiers.size)
        assertEquals(26.0, mining.modifiers.single().amount)
        assertEquals(UpdateAttributesPacket.Operation.ADD_VALUE, mining.modifiers.single().operation)
    }

    /** An empty hand still sends every attribute, so the last modifier does not linger. */
    @Test
    fun anEmptyHandSendsTheAttributesWithNoModifiers() {
        val snapshots = EnchantmentAttributes.snapshotsFor(null, ::efficiencyDefinition)

        assertTrue(snapshots.isNotEmpty())
        assertTrue(snapshots.all { it.modifiers.isEmpty() })
        assertEquals(0.0, snapshots.single { it.attribute == MINING_EFFICIENCY }.base)
    }
}
