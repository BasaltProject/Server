package org.basaltmc.registry

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Spot checks against values anyone can verify in the game. The table is generated from the real
 * jar, so this is not re-testing the extraction: it is a tripwire for a regeneration that silently
 * produced garbage, which is exactly what happened once when the harness's output arrived wrapped
 * in the game's own log prefixes.
 */
class BlockHardnessTest {
    @Test
    fun stoneNeedsAPickaxeAndTakesRealTime() {
        val stone = Blocks.hardnessOf(Identifier.vanilla("stone"))
        assertEquals(1.5f, stone?.destroyTime)
        assertTrue(stone?.requiresCorrectTool == true)
        assertFalse(stone?.isInstant == true)
    }

    @Test
    fun flowersAndGrassComeApartInstantly() {
        // These are the blocks a client destroys on the spot without ever sending the end of a dig.
        assertTrue(Blocks.hardnessOf(Identifier.vanilla("dandelion"))?.isInstant == true)
        assertTrue(Blocks.hardnessOf(Identifier.vanilla("short_grass"))?.isInstant == true)
    }

    @Test
    fun bedrockIsUnbreakable() {
        val bedrock = Blocks.hardnessOf(Identifier.vanilla("bedrock"))
        assertTrue(bedrock?.isUnbreakable == true)
        assertFalse(bedrock?.isInstant == true)
    }

    @Test
    fun leavesAreQuickButNotInstant() {
        val leaves = Blocks.hardnessOf(Identifier.vanilla("oak_leaves"))
        assertEquals(0.2f, leaves?.destroyTime)
        assertFalse(leaves?.isInstant == true)
    }

    @Test
    fun everyRegisteredBlockHasAHardness() {
        // The table and the block registry come out of the same jar in the same run, so a block
        // missing from one of them means the generation went wrong.
        val missing =
            Blocks.REGISTRY
                .values()
                .map { it.block }
                .filter { Blocks.hardnessOf(it) == null }
        assertTrue(missing.isEmpty(), "blocks with no hardness entry: ${missing.take(5)}")
    }

    @Test
    fun somethingThatIsNotABlockHasNoHardness() {
        assertNull(Blocks.hardnessOf(Identifier.vanilla("not_a_real_block")))
    }
}

/**
 * Block ids and block *state* ids are different numberings, and using one where the other belongs
 * is silently wrong rather than an error: a tag full of state ids names entirely the wrong blocks.
 */
class BlockIdTest {
    @Test
    fun blockIdsAreNotBlockStateIds() {
        // Air and stone happen to agree; anything with properties before it does not.
        assertEquals(0, Blocks.blockIdOf(Identifier.vanilla("air")))
        assertEquals(1, Blocks.blockIdOf(Identifier.vanilla("stone")))

        val dirt = Identifier.vanilla("dirt")
        assertEquals(9, Blocks.blockIdOf(dirt))
        // Its *state* id is 10: one apart, which is exactly why mixing them up is hard to spot.
        assertEquals(10, Blocks.REGISTRY.idOf(dirt))
    }

    @Test
    fun everyRegisteredBlockHasABlockId() {
        val missing =
            Blocks.REGISTRY
                .values()
                .map { it.block }
                .filter { Blocks.blockIdOf(it) == null }
        assertTrue(missing.isEmpty(), "blocks with no registry id: ${missing.take(5)}")
    }
}
