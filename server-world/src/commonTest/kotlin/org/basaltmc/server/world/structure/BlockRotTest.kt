package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * `block_rot` is what makes an overgrown overlay a scattering rather than a second copy of the
 * building. At integrity 0.05 it must keep about one block in twenty; keeping all of them looks
 * like a working structure until you compare it with the jar.
 */
class BlockRotTest {
    private val cobblestone =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mossy_cobblestone"))?.stateId
            ?: error("no mossy cobblestone")

    @Test
    fun `the outpost's own processor list parses to a five percent rot`() {
        // parity: data/minecraft/worldgen/processor_list/outpost_rot.json, verbatim.
        val list =
            VillagePoolJson.parseProcessorList(
                """{"processors":[{"integrity":0.05,"processor_type":"minecraft:block_rot"}]}""",
            )
        val rot = list.rot ?: error("block_rot was not parsed")
        assertEquals(0.05f, rot.integrity)

        var kept = 0
        val total = 4000
        for (i in 0 until total) {
            if (rot.keeps(cobblestone, StructurePos(i % 64, 70 + i / 64, -900 - i % 37))) kept++
        }
        val rate = kept.toDouble() / total
        assertTrue(rate in 0.02..0.09, "kept $kept of $total ($rate), expected about a twentieth")
    }

    @Test
    fun `a block outside the rottable tag is never dropped and costs no draw`() {
        val rot = BlockRot(0.0f, setOf(cobblestone))
        val other =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dark_oak_log"))?.stateId ?: error("no log")
        assertTrue(rot.keeps(other, StructurePos(1, 2, 3)))
    }

    @Test
    fun `a list element keeps each sub-element's own processors`() {
        // parity: the shape of `pillager_outpost/towers.json`, where the plain watchtower carries
        // no processors and the overgrown overlay on top of it carries `outpost_rot`.
        val pool =
            VillagePoolJson.parsePool(
                POOL_JSON,
                { null },
            ) { name ->
                if (name == "minecraft:outpost_rot") {
                    """{"processors":[{"integrity":0.05,"processor_type":"minecraft:block_rot"}]}"""
                } else {
                    null
                }
            }
        val list = pool.getRandomTemplate(WorldgenRandom(LegacyRandom(0L))) as ListPoolElement
        val subs = list.elements.map { it as LegacySinglePoolElement }
        assertEquals(2, subs.size)
        assertEquals(null, subs[0].processors.rot, "the plain watchtower must not rot")
        assertEquals(0.05f, subs[1].processors.rot?.integrity, "the overgrown overlay lost its rot")
    }

    @Test
    fun `a list with only a rot is not an empty list`() {
        // The placement path skips a list it thinks is empty, and `outpost_rot` carries no rules
        // at all, so asking `rules.isEmpty()` there silently placed the whole overgrown overlay.
        val rotOnly =
            VillagePoolJson.parseProcessorList(
                """{"processors":[{"integrity":0.05,"processor_type":"minecraft:block_rot"}]}""",
            )
        assertTrue(rotOnly.rules.isEmpty(), "outpost_rot has no rules, which is the whole trap")
        assertTrue(!rotOnly.isEmpty, "a rot-only list still has work to do")
        assertTrue(StructureProcessorList.EMPTY.isEmpty)
    }

    @Test
    fun `a protected block in the world keeps the whole list out`() {
        // parity: every one of the jar's seven lists that carries `protected_blocks` puts it last,
        // and it draws nothing, so it reads as a final veto on what the world already holds.
        val chest = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chest"))?.stateId ?: error("no chest")
        val stone = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))?.stateId ?: error("no stone")
        val list =
            VillagePoolJson.parseProcessorList(
                """{"processors":[{"processor_type":"minecraft:protected_blocks",""" +
                    """"value":"#minecraft:features_cannot_replace"}]}""",
            ) { if (it == "minecraft:features_cannot_replace") setOf(chest) else null }
        assertTrue(!list.isEmpty, "a protection-only list still has work to do")
        assertEquals(null, list.processAt(stone, chest, StructurePos(1, 2, 3)), "a chest must survive")
        assertEquals(stone, list.processAt(stone, stone, StructurePos(1, 2, 3)), "plain stone must not")
    }

    private companion object {
        /** The shape of `pillager_outpost/towers.json`, trimmed to the two sub-elements. */
        const val POOL_JSON: String =
            """{"elements":[{"element":{"element_type":"minecraft:list_pool_element","projection":"rigid",""" +
                """"elements":[{"element_type":"minecraft:legacy_single_pool_element","location":"minecraft:a",""" +
                """"processors":{"processors":[]},"projection":"rigid"},""" +
                """{"element_type":"minecraft:legacy_single_pool_element","location":"minecraft:b",""" +
                """"processors":"minecraft:outpost_rot","projection":"rigid"}]},"weight":1}],""" +
                """"fallback":"minecraft:empty"}"""
    }
}
