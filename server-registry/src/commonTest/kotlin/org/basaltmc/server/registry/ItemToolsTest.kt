package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The rules come from the jar; what is worth testing is that they are applied in vanilla's own
 * order, because "first matching rule wins" is the whole reason a tier exclusion works at all.
 */
class ItemToolsTest {
    private val stone = Identifier.vanilla("stone")
    private val obsidian = Identifier.vanilla("obsidian")

    /** Stands in for the real tags: obsidian is the block a wooden pickaxe is excluded from. */
    private fun tagContains(
        tag: String,
        block: Identifier,
    ): Boolean =
        when (tag) {
            "minecraft:mineable/pickaxe" -> block == stone || block == obsidian
            "minecraft:incorrect_for_wooden_tool" -> block == obsidian
            else -> false
        }

    @Test
    fun aPickaxeIsTheCorrectToolForStone() {
        assertTrue(ItemTools.isCorrectForDrops(Identifier.vanilla("diamond_pickaxe"), stone, ::tagContains))
        assertTrue(ItemTools.isCorrectForDrops(Identifier.vanilla("wooden_pickaxe"), stone, ::tagContains))
    }

    @Test
    fun theTierExclusionWinsBecauseItIsListedFirst() {
        // Obsidian is in both `mineable/pickaxe` and `incorrect_for_wooden_tool`. Vanilla takes the
        // first matching rule, and the exclusion comes first, so a wooden pickaxe gets nothing.
        assertFalse(ItemTools.isCorrectForDrops(Identifier.vanilla("wooden_pickaxe"), obsidian, ::tagContains))
        assertTrue(ItemTools.isCorrectForDrops(Identifier.vanilla("diamond_pickaxe"), obsidian, ::tagContains))
    }

    @Test
    fun aBareHandOrAnUnrelatedItemIsNeverCorrect() {
        assertFalse(ItemTools.isCorrectForDrops(null, stone, ::tagContains))
        assertFalse(ItemTools.isCorrectForDrops(Identifier.vanilla("diamond"), stone, ::tagContains))
    }

    @Test
    fun miningSpeedComesFromTheMatchingRule() {
        // The real values: a diamond pickaxe mines eight times as fast as a hand, wood twice.
        assertEquals(8.0f, ItemTools.miningSpeed(Identifier.vanilla("diamond_pickaxe"), stone, ::tagContains))
        assertEquals(2.0f, ItemTools.miningSpeed(Identifier.vanilla("wooden_pickaxe"), stone, ::tagContains))
        assertEquals(1.0f, ItemTools.miningSpeed(null, stone, ::tagContains))
    }
}
