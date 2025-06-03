package org.basaltmc.world.advancement

import org.basaltmc.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class AdvancementTest {
    @Test
    fun defaultRequirementsAreAnAndOfEveryCriterion() {
        val advancement =
            AdvancementJson.parse(
                """
                {
                    "criteria": {
                        "a": { "trigger": "minecraft:tick" },
                        "b": { "trigger": "minecraft:tick" }
                    }
                }
                """.trimIndent(),
            )

        assertFalse(advancement.isComplete(setOf("a")))
        assertTrue(advancement.isComplete(setOf("a", "b")))
    }

    @Test
    fun explicitOrGroupNeedsOnlyOneMember() {
        val advancement =
            AdvancementJson.parse(
                """
                {
                    "criteria": {
                        "a": { "trigger": "minecraft:tick" },
                        "b": { "trigger": "minecraft:tick" }
                    },
                    "requirements": [["a", "b"]]
                }
                """.trimIndent(),
            )

        assertTrue(advancement.isComplete(setOf("a")))
        assertTrue(advancement.isComplete(setOf("b")))
        assertFalse(advancement.isComplete(emptySet()))
    }

    @Test
    fun anAdvancementWithNoGroupsIsNeverComplete() {
        assertFalse(AdvancementRequirements(emptyList()).isComplete(setOf("anything")))
    }

    @Test
    fun parsesParentRewardsAndCriteriaTriggers() {
        val advancement =
            AdvancementJson.parse(
                """
                {
                    "parent": "minecraft:story/root",
                    "criteria": {
                        "consumed_item": { "trigger": "minecraft:inventory_changed", "conditions": {} }
                    },
                    "rewards": {
                        "experience": 10,
                        "recipes": ["minecraft:stick"]
                    }
                }
                """.trimIndent(),
            )

        assertEquals(Identifier.vanilla("story/root"), advancement.parent)
        assertEquals(Identifier.vanilla("inventory_changed"), advancement.criteria.getValue("consumed_item").trigger)
        assertEquals(10, advancement.rewards.experience)
        assertEquals(listOf(Identifier.vanilla("stick")), advancement.rewards.recipes)
    }

    @Test
    fun rootAdvancementsHaveNoParent() {
        val advancement = AdvancementJson.parse("""{"criteria": {"a": {"trigger": "minecraft:tick"}}}""")
        assertNull(advancement.parent)
    }
}
