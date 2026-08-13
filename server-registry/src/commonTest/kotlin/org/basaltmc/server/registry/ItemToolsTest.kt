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

    @Test
    fun efficiencyIsWhatMakesAPickaxeBreakStoneInOneTick() {
        val diamondPickaxe = Identifier.vanilla("diamond_pickaxe")

        // Plain: stone is hardness 1.5, so 8 / 1.5 / 30 is nowhere near a whole tick.
        val plain =
            ItemTools.destroyProgressPerTick(
                diamondPickaxe,
                stone,
                hardness = 1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
                tagContains = ::tagContains,
            )
        assertTrue(plain < 1f)

        // Efficiency 50 adds 2501, which is what makes it instant and what the server has to agree
        // with: the client stops sending the end of a dig it finished in one tick.
        val enchanted =
            ItemTools.destroyProgressPerTick(
                diamondPickaxe,
                stone,
                hardness = 1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 50,
                tagContains = ::tagContains,
            )
        assertTrue(enchanted >= 1f)

        // parity: the enchantment does nothing for a bare hand, whose speed is never above one.
        val barehand =
            ItemTools.destroyProgressPerTick(
                null,
                stone,
                hardness = 1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 50,
                tagContains = ::tagContains,
            )
        assertTrue(barehand < 1f)
    }
}
