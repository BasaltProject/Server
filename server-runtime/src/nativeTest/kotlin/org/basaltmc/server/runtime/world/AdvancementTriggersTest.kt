package org.basaltmc.server.runtime.world

import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.world.advancement.AdvancementJson
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The two triggers this build can observe, against the JSON vanilla actually ships: `story/root`'s
 * crafting table, `story/mine_stone`'s tag of stone materials, and a recipe unlock.
 */
class AdvancementTriggersTest {
    private val root =
        AdvancementJson.parse(
            """
            {
              "criteria": {
                "crafting_table": {
                  "conditions": {"items": [{"items": "minecraft:crafting_table"}]},
                  "trigger": "minecraft:inventory_changed"
                }
              },
              "display": {
                "title": {"translate": "advancements.story.root.title"},
                "description": {"translate": "advancements.story.root.description"}
              }
            }
            """.trimIndent(),
        )

    private val mineStone =
        AdvancementJson.parse(
            """
            {
              "parent": "minecraft:story/root",
              "criteria": {
                "get_stone": {
                  "conditions": {"items": [{"items": "#minecraft:stone_tool_materials"}]},
                  "trigger": "minecraft:inventory_changed"
                }
              },
              "rewards": {"experience": 5, "recipes": ["minecraft:stone_pickaxe"]}
            }
            """.trimIndent(),
        )

    private val unlocked =
        AdvancementJson.parse(
            """
            {
              "criteria": {
                "has_the_recipe": {
                  "conditions": {"recipe": "minecraft:stone_pickaxe"},
                  "trigger": "minecraft:recipe_unlocked"
                }
              }
            }
            """.trimIndent(),
        )

    private val definitions =
        mapOf(
            Identifier.vanilla("story/root") to root,
            Identifier.vanilla("story/mine_stone") to mineStone,
            Identifier.vanilla("recipes/stone_pickaxe") to unlocked,
        )

    private val progress = PlayerProgress()
    private val announcements = mutableListOf<String>()
    private val experience = mutableListOf<Int>()
    private val recipesTaught = mutableListOf<Identifier>()

    private val triggers =
        AdvancementTriggers(
            advancements = RealAdvancements(serverJar = null, definitions = definitions),
            progress = progress,
            itemTagMembers = { tag ->
                if (tag ==
                    Identifier.vanilla("stone_tool_materials")
                ) {
                    setOf(Identifier.vanilla("cobblestone"))
                } else {
                    emptySet()
                }
            },
            announce = { _, message -> announcements += PlainTextSerializer.serialize(message) },
            awardExperience = { _, amount -> experience += amount },
            awardRecipe = { _, recipe -> recipesTaught += recipe },
        )

    private fun inventoryOf(vararg items: String) = items.map { StoredItemStack(it, 1) }

    @Test
    fun anItemInTheInventoryCompletesItsAdvancement() {
        triggers.onInventoryChanged("Whiron", UUID, inventoryOf("minecraft:crafting_table"))
        assertEquals(setOf("crafting_table"), progress.grantedCriteria(UUID, Identifier.vanilla("story/root")))
        assertTrue(announcements.any { "chat.type.advancement.task" in it })
    }

    @Test
    fun aTagMatcherResolvesThroughTheTagTable() {
        triggers.onInventoryChanged("Whiron", UUID, inventoryOf("minecraft:cobblestone"))
        assertEquals(setOf("get_stone"), progress.grantedCriteria(UUID, Identifier.vanilla("story/mine_stone")))
    }

    @Test
    fun completingOneGrantsItsRewardsExactlyOnce() {
        triggers.onInventoryChanged("Whiron", UUID, inventoryOf("minecraft:cobblestone"))
        triggers.onInventoryChanged("Whiron", UUID, inventoryOf("minecraft:cobblestone"))
        assertEquals(listOf(5), experience)
        assertEquals(listOf(Identifier.vanilla("stone_pickaxe")), recipesTaught)
    }

    @Test
    fun anItemNobodyIsWaitingForChangesNothing() {
        triggers.onInventoryChanged("Whiron", UUID, inventoryOf("minecraft:dirt"))
        assertTrue(announcements.isEmpty())
        assertTrue(progress.grantedCriteria(UUID, Identifier.vanilla("story/root")).isEmpty())
    }

    @Test
    fun unlockingARecipeCompletesItsOwnAdvancement() {
        triggers.onRecipeUnlocked("Whiron", UUID, Identifier.vanilla("stone_pickaxe"))
        assertEquals(
            setOf("has_the_recipe"),
            progress.grantedCriteria(UUID, Identifier.vanilla("recipes/stone_pickaxe")),
        )
    }

    @Test
    fun anAdvancementWithNoDisplayIsCompletedInSilence() {
        triggers.onRecipeUnlocked("Whiron", UUID, Identifier.vanilla("stone_pickaxe"))
        assertTrue(announcements.isEmpty())
    }

    private companion object {
        const val UUID = "11111111-2222-3333-4444-555555555555"
    }
}
