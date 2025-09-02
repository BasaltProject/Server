package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A processor that drops a block and one that replaces it are easy to confuse, and getting it wrong
 * shows up as a structure that quietly overwrites whatever it lands on.
 */
class TemplateBlockProcessorsTest {
    private fun state(path: String) =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun path(stateId: Int) = Blocks.byStateId(stateId)?.block?.path

    private val here = StructurePos(12, 40, -7)

    @Test
    fun `a protected block in the world keeps the structure out`() {
        val chest = state("chest")
        assertNull(TemplateBlockProcessors.PROTECT_FEATURES.process(state("stone"), chest, here))
        assertEquals(
            state("stone"),
            TemplateBlockProcessors.PROTECT_FEATURES.process(state("stone"), state("dirt"), here),
        )
    }

    @Test
    fun `the structure void is dropped and its air only by the stricter one`() {
        val air = Blocks.AIR.stateId
        val void = state("structure_void")
        assertNull(TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK.process(void, air, here))
        assertEquals(air, TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK.process(air, air, here))
        assertNull(TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR.process(air, air, here))
    }

    @Test
    fun `something not solid keeps the lava it is placed into`() {
        val lava = state("lava")
        val slab = state("stone_brick_slab")
        assertEquals(lava, TemplateBlockProcessors.LAVA_SUBMERGED.process(slab, lava, here))
        val stone = state("stone")
        assertEquals(stone, TemplateBlockProcessors.LAVA_SUBMERGED.process(stone, lava, here))
    }

    @Test
    fun `blackstone replacement keeps the stair it replaces facing the same way`() {
        val stairs =
            Blocks.stateIdOf(
                Identifier.vanilla("cobblestone_stairs"),
                mapOf("facing" to "west", "half" to "top", "shape" to "straight", "waterlogged" to "false"),
            ) ?: error("no cobblestone stairs state")
        val replaced = TemplateBlockProcessors.BLACKSTONE_REPLACE.process(stairs, Blocks.AIR.stateId, here)
        assertEquals("blackstone_stairs", path(replaced!!))
        assertEquals("west", Blocks.byStateId(replaced)?.properties?.get("facing"))
        assertEquals("top", Blocks.byStateId(replaced)?.properties?.get("half"))
    }

    @Test
    fun `ageing is decided by the position and not by the order blocks arrive in`() {
        val bricks = state("stone_bricks")
        val aged = TemplateBlockProcessors.age(1.0f)
        val first = aged.process(bricks, Blocks.AIR.stateId, here)
        val again = aged.process(bricks, Blocks.AIR.stateId, here)
        assertEquals(first, again, "the same block at the same place must age the same way")
        val elsewhere = aged.process(bricks, Blocks.AIR.stateId, StructurePos(13, 40, -7))
        assertTrue(first != null && elsewhere != null)
    }

    @Test
    fun `full mossiness never ages a brick into a dry replacement`() {
        val aged = TemplateBlockProcessors.age(1.0f)
        var replacements = 0
        for (x in 0 until 200) {
            val result = aged.process(state("stone_bricks"), Blocks.AIR.stateId, StructurePos(x, 40, 0))
            val resultPath = path(result!!)
            if (resultPath == "stone_bricks") continue
            replacements++
            assertTrue(
                resultPath!!.startsWith("mossy_"),
                "mossiness 1.0 must only ever pick a mossy replacement, not $resultPath",
            )
        }
        assertTrue(replacements > 50, "about half the bricks should age, got $replacements of 200")
    }

    @Test
    fun `a chain stops at the first processor that drops the block`() {
        val chain =
            TemplateBlockProcessorChain(
                listOf(
                    TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR,
                    TemplateBlockProcessors.BLACKSTONE_REPLACE,
                ),
            )
        assertNull(chain.process(Blocks.AIR.stateId, Blocks.AIR.stateId, here))
        assertEquals("blackstone", path(chain.process(state("cobblestone"), Blocks.AIR.stateId, here)!!))
    }
}
