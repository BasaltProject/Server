package org.basaltmc.server.runtime.item

import kotlinx.serialization.json.Json
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.JsonToNbt
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The `minecraft:item_damage` effect, with unbreaking's own definition written out the way the
 * jar ships it: a fraction chance per level, rolled once per point of wear.
 */
class ItemDamageEffectsTest {
    private val unbreaking =
        JsonToNbt.convert(
            Json.parseToJsonElement(
                """
                {
                  "effects": {
                    "minecraft:item_damage": [
                      {
                        "effect": {
                          "type": "minecraft:remove_binomial",
                          "chance": {
                            "type": "minecraft:fraction",
                            "denominator": {"type": "minecraft:linear", "base": 2.0, "per_level_above_first": 1.0},
                            "numerator": {"type": "minecraft:linear", "base": 1.0, "per_level_above_first": 0.0}
                          }
                        },
                        "requirements": {
                          "condition": "minecraft:match_tool",
                          "predicate": {"items": "#minecraft:enchantable/durability"}
                        }
                      }
                    ]
                  }
                }
                """.trimIndent(),
            ),
        ) as NbtCompound

    private fun effects(inTag: Boolean = true) =
        ItemDamageEffects(
            definitionOf = { if (it == Identifier.vanilla("unbreaking")) unbreaking else null },
            itemInTag = { _, _ -> inTag },
        )

    private fun enchantedPickaxe(level: Int): StoredItemStack {
        val enchantments = NbtCompound()
        enchantments["minecraft:unbreaking"] = NbtInt(level)
        val components = NbtCompound()
        components["minecraft:enchantments"] = enchantments
        return StoredItemStack("minecraft:diamond_pickaxe", 1, components)
    }

    @Test
    fun unbreakingRemovesSomeOfTheWearOverManyUses() {
        val random = LegacyRandom(42L)
        val stack = enchantedPickaxe(3)
        var applied = 0
        repeat(USES) { applied += effects().process(stack, 1, random) }
        // Level 3 is a 1-in-4 chance per point, so most uses still cost one and some cost none.
        assertTrue(applied in (USES / 2)..(USES - 1), "$applied of $USES uses cost durability")
    }

    @Test
    fun anItemOutsideTheEffectsTagIsUntouched() {
        val random = LegacyRandom(42L)
        assertEquals(1, effects(inTag = false).process(enchantedPickaxe(3), 1, random))
    }

    @Test
    fun anUnenchantedItemPaysTheFullCost() {
        val random = LegacyRandom(42L)
        assertEquals(2, effects().process(StoredItemStack("minecraft:diamond_pickaxe", 1), 2, random))
    }

    private companion object {
        const val USES = 400
    }
}
