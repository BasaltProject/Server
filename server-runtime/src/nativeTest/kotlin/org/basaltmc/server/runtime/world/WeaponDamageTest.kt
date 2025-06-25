package org.basaltmc.server.runtime.world

import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `EnchantmentHelper.modifyDamage` over the `minecraft:damage` effects. */
class WeaponDamageTest {
    /** parity: sharpness, whose effect is `AddValue(perLevel(1.0, 0.5))` and asks nothing of the target. */
    private val sharpness =
        compound(
            "effects" to
                compound(
                    "minecraft:damage" to
                        NbtList(
                            NbtType.COMPOUND,
                            listOf(
                                compound(
                                    "effect" to
                                        compound(
                                            "type" to NbtString("minecraft:add"),
                                            "value" to
                                                compound(
                                                    "base" to NbtDouble(1.0),
                                                    "per_level_above_first" to NbtDouble(0.5),
                                                ),
                                        ),
                                ),
                            ),
                        ),
                ),
        )

    private fun compound(vararg entries: Pair<String, org.basaltmc.wire.nbt.NbtTag>): NbtCompound =
        NbtCompound(entries.toMap())

    private fun sword(level: Int): StoredItemStack =
        StoredItemStack(
            id = "minecraft:iron_sword",
            count = 1,
            components = compound("minecraft:enchantments" to compound("minecraft:sharpness" to NbtInt(level))),
        )

    private fun definitionOf(id: Identifier): NbtCompound? =
        if (id == Identifier.vanilla("sharpness")) sharpness else null

    @Test
    fun aPlainWeaponAddsNothing() {
        assertEquals(6f, WeaponDamage.of(null, 6f, ::definitionOf))
        assertEquals(6f, WeaponDamage.of(StoredItemStack("minecraft:iron_sword", 1), 6f, ::definitionOf))
    }

    @Test
    fun sharpnessAddsOneAndAHalfPerLevelAfterTheFirst() {
        assertEquals(7f, WeaponDamage.of(sword(1), 6f, ::definitionOf))
        assertEquals(8f, WeaponDamage.of(sword(3), 6f, ::definitionOf))
    }
}
