package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the structure processor rules. */
class StructureProcessorOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val cobblestone = stateId("cobblestone")
    private val mossy = stateId("mossy_cobblestone")
    private val stoneBricks = stateId("stone_bricks")
    private val crackedBricks = stateId("cracked_stone_bricks")

    @Test
    fun dumpTests() {
        val snow = stateId("snow")
        val tests =
            listOf(
                "blockMatch" to StructureRuleTest.BlockMatch(snow),
                "randomBlockMatch" to StructureRuleTest.RandomBlockMatch(snow, 0.5f),
                "blockStateMatch" to StructureRuleTest.BlockStateMatch(snow),
                "tagMatch" to StructureRuleTest.TagMatch(setOf(cobblestone, snow)),
            )
        // Snow carries a layer count, so `snow + 1` is the same block in a different state: that is
        // what separates matching a block from matching one exact state.
        val probes = listOf(snow, snow + 1, cobblestone, stoneBricks, Blocks.AIR.stateId)
        for ((name, test) in tests) {
            val random = WorldgenRandom(LegacyRandom(4242L))
            val results = StringBuilder()
            for (probe in probes) {
                repeat(3) { results.append(if (test.test(probe, random)) '1' else '0') }
            }
            println("ORACLE test_$name results=$results after=${random.nextInt()}")
        }
    }

    @Test
    fun dumpProcessing() {
        val rules =
            listOf(
                StructureProcessorRule(
                    StructureRuleTest.RandomBlockMatch(cobblestone, 0.5f),
                    null,
                    mossy,
                ),
                StructureProcessorRule(
                    StructureRuleTest.BlockMatch(stoneBricks),
                    StructureRuleTest.BlockMatch(Blocks.WATER.stateId),
                    crackedBricks,
                ),
            )
        val list =
            StructureProcessorList(
                rules,
                rot = BlockRot(0.6f, setOf(cobblestone)),
                protectedBlocks = setOf(stateId("bedrock")),
            )
        var checksum = -3750763034362895579L
        var dropped = 0
        var changed = 0
        for (index in 0 until 512) {
            val input = listOf(cobblestone, stoneBricks, mossy)[index.mod(3)]
            val world = listOf(Blocks.AIR.stateId, Blocks.WATER.stateId, stateId("bedrock"))[index.mod(3)]
            val position = StructurePos(index.mod(29) - 14, index.mod(17) + 40, index.mod(23) - 11)
            val result = list.processAt(input, world, position)
            if (result == null) {
                dropped++
            } else if (result != input) {
                changed++
            }
            checksum = (checksum xor (result?.toLong() ?: -1L)) * 1099511628211L
        }
        println("ORACLE processing dropped=$dropped changed=$changed checksum=$checksum empty=${list.isEmpty}")
        println("ORACLE emptyList empty=${StructureProcessorList.EMPTY.isEmpty}")

        val mossify = StructureProcessorList.mossify(0.7f)
        var mossChecksum = -3750763034362895579L
        for (index in 0 until 256) {
            val position = StructurePos(index.mod(31) - 15, 64, index.mod(19) - 9)
            val result = mossify.processAt(cobblestone, Blocks.AIR.stateId, position)
            mossChecksum = (mossChecksum xor (result?.toLong() ?: -1L)) * 1099511628211L
        }
        println("ORACLE mossify checksum=$mossChecksum empty=${mossify.isEmpty}")
    }

    @Test
    fun dumpCapped() {
        val capped =
            CappedRules(
                listOf(
                    StructureProcessorRule(StructureRuleTest.BlockMatch(cobblestone), null, mossy),
                    StructureProcessorRule(StructureRuleTest.BlockMatch(stoneBricks), null, crackedBricks),
                    // A rule whose output is its own input: it matches and still converts nothing,
                    // so it must not eat one of the six slots.
                    StructureProcessorRule(StructureRuleTest.BlockMatch(mossy), null, mossy),
                ),
                limit = 18,
            )
        val candidates = (0 until 40).map { listOf(cobblestone, stoneBricks, mossy, Blocks.AIR.stateId)[it.mod(4)] }
        val chosen = capped.choose(candidates, 7777L, StructurePos(12, 64, -30))
        val rendered = chosen.keys.sorted().joinToString(",") { "$it=${chosen[it]!!.outputStateId}" }
        println("ORACLE capped size=${chosen.size} chosen=$rendered")
        val none = CappedRules(emptyList(), limit = 0).choose(candidates, 7777L, StructurePos(0, 0, 0))
        println("ORACLE cappedNone size=${none.size}")
    }
}
