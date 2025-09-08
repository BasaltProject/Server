package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.PositionalHashing
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.RuleTest, narrowed to
 * the predicate kinds the real village processor lists actually use: `random_block_match` (a fixed
 * block, accepted with [probability] chance), `block_match` (any state of a fixed block),
 * `blockstate_match` (one exact property-bearing state) and
 * `tag_match` (any block of a named tag, e.g. zombie villages' `minecraft:doors` rule).
 * `minecraft:always_true` is the null/no-op predicate: this project represents it as `null` rather
 * than a dedicated object.
 */
public sealed class StructureRuleTest {
    public abstract fun test(
        stateId: Int,
        random: WorldgenRandom,
    ): Boolean

    public class BlockMatch(
        public val blockStateId: Int,
    ) : StructureRuleTest() {
        private val block = Blocks.byStateId(blockStateId)?.block

        override fun test(
            stateId: Int,
            random: WorldgenRandom,
        ): Boolean = block != null && Blocks.byStateId(stateId)?.block == block
    }

    public class RandomBlockMatch(
        public val blockStateId: Int,
        public val probability: Float,
    ) : StructureRuleTest() {
        private val block = Blocks.byStateId(blockStateId)?.block

        override fun test(
            stateId: Int,
            random: WorldgenRandom,
        ): Boolean = block != null && Blocks.byStateId(stateId)?.block == block && random.nextFloat() < probability
    }

    public class BlockStateMatch(
        public val stateId: Int,
    ) : StructureRuleTest() {
        override fun test(
            stateId: Int,
            random: WorldgenRandom,
        ): Boolean = stateId == this.stateId
    }

    /** // parity: net.minecraft.world.level.levelgen.structure.templatesystem.TagMatchTest (zombie village pools' `minecraft:doors` rule). */
    public class TagMatch(
        public val stateIds: Set<Int>,
    ) : StructureRuleTest() {
        private val blocks = stateIds.mapNotNull { Blocks.byStateId(it)?.block }.toSet()

        override fun test(
            stateId: Int,
            random: WorldgenRandom,
        ): Boolean = Blocks.byStateId(stateId)?.block in blocks
    }
}

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.ProcessorRule. [inputTest]
 * checks the structure's own (unplaced) block; [locationTest], if present, checks whatever block
 * is already in the world at the target position (`null` mirrors `always_true`: no location
 * constraint, matching every real Plains rule except `street_plains`'s water-detection rules).
 */
public class StructureProcessorRule(
    public val inputTest: StructureRuleTest,
    public val locationTest: StructureRuleTest?,
    public val outputStateId: Int,
    /** // parity: `block_entity_modifier: append_loot`, the loot table the output block gets. */
    public val appendLootTable: String? = null,
) {
    public fun apply(
        inputStateId: Int,
        worldStateId: Int,
        random: WorldgenRandom,
    ): Int? {
        if (!inputTest.test(inputStateId, random)) return null
        if (locationTest != null && !locationTest.test(worldStateId, random)) return null
        return outputStateId
    }
}

/**
 * // parity: BlockRotProcessor. Drops a share of the blocks it is allowed to rot, which is what
 * makes a ruin look ruined. It draws only for a block the tag covers, so a wall of something else
 * costs no randomness at all.
 */
public class BlockRot(
    public val integrity: Float,
    public val rottable: Set<Int>?,
) {
    public fun keeps(
        stateId: Int,
        position: StructurePos,
        /** The placement's own stream when it set one. // parity: `StructurePlaceSettings.getRandom`. */
        shared: WorldgenRandom? = null,
    ): Boolean {
        if (rottable != null && stateId !in rottable) return true
        val random =
            shared ?: WorldgenRandom(LegacyRandom(PositionalHashing.getSeed(position.x, position.y, position.z)))
        return random.nextFloat() <= integrity
    }
}

/**
 * // parity: `CappedProcessor`, a second pass converting at most [limit] blocks. It evaluates the
 * piece's whole list rather than the part inside the chunk, and its random is a positional fork of
 * the world seed, so it neither consumes from the feature stream nor depends on it.
 */
public class CappedRules(
    public val delegate: List<StructureProcessorRule>,
    public val limit: Int,
) {
    /**
     * The indices of [candidates] this converts, in the order the shuffle offers them, stopping at
     * [limit] actual conversions. // parity: `finalizeProcessing`, which counts only the blocks the
     * delegate really changed, not the ones it looked at.
     */
    public fun choose(
        candidates: List<Int>,
        worldSeed: Long,
        position: StructurePos,
    ): Map<Int, StructureProcessorRule> {
        if (limit == 0 || candidates.isEmpty()) return emptyMap()
        val factorySeed = LegacyRandom(worldSeed).nextLong()
        val random =
            WorldgenRandom(
                LegacyRandom(PositionalHashing.getSeed(position.x, position.y, position.z) xor factorySeed),
            )
        val maxToReplace = minOf(limit, candidates.size)
        val order = shuffledCopy(candidates.indices.toList(), random)
        val chosen = mutableMapOf<Int, StructureProcessorRule>()
        for (index in order) {
            if (chosen.size >= maxToReplace) break
            val stateId = candidates[index]
            val rule = delegate.firstOrNull { it.inputTest.test(stateId, random) && it.outputStateId != stateId }
            if (rule != null) chosen[index] = rule
        }
        return chosen
    }
}

/**
 * // parity: RuleProcessor, first matching rule wins, preceded by a BlockRotProcessor. Each takes its
 * own random from the same positional seed rather than sharing a stream.
 */
public class StructureProcessorList(
    public val rules: List<StructureProcessorRule>,
    public val rot: BlockRot? = null,
    /**
     * // parity: ProtectedBlockProcessor. Whatever the world already holds here and lists in the
     * tag survives: a structure never eats another one's chest, spawner, vault or bedrock. It is
     * last in all seven of the jar's lists that carry it, and it draws nothing.
     */
    public val protectedBlocks: Set<Int>? = null,
    /**
     * // parity: the CappedProcessors of the list, in order. A list can carry more than one, and
     * `trail_ruins_houses_archaeology` does: six blocks of common loot and then three more of rare,
     * the second choosing from what the first left alone.
     */
    public val capped: List<CappedRules> = emptyList(),
) {
    /**
     * Whether this list does anything at all. A caller must ask this and not `rules.isEmpty()`: a
     * list can carry only a `block_rot`, which has no rules and still drops most of the blocks.
     */
    public val isEmpty: Boolean get() = rules.isEmpty() && rot == null && protectedBlocks == null && capped.isEmpty()

    public fun process(
        inputStateId: Int,
        worldStateId: Int,
        random: WorldgenRandom,
    ): Int {
        for (rule in rules) {
            val result = rule.apply(inputStateId, worldStateId, random)
            if (result != null) return result
        }
        return inputStateId
    }

    /**
     * The state to place, or null when the rot dropped this block.
     *
     * // parity: RuleProcessor creates a fresh LegacyRandom from Mth.getSeed at the transformed
     * world position for every block. Structure placement's shared random stream is not consumed.
     */

    public fun processAt(
        inputStateId: Int,
        worldStateId: Int,
        position: StructurePos,
        /**
         * The placement's own stream, for a caller that set one. // parity: `StructurePlaceSettings.getRandom`
         * only falls back to a fresh positional source when there is none.
         */
        shared: WorldgenRandom? = null,
    ): Int? {
        if (rot?.keeps(inputStateId, position, shared) == false) return null
        if (protectedBlocks != null && worldStateId in protectedBlocks) return null
        return process(
            inputStateId,
            worldStateId,
            shared ?: WorldgenRandom(LegacyRandom(PositionalHashing.getSeed(position.x, position.y, position.z))),
        )
    }

    public companion object {
        public val EMPTY: StructureProcessorList = StructureProcessorList(emptyList())

        private fun blockId(name: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.parse(name))?.stateId ?: Blocks.AIR.stateId

        /** // parity: data/minecraft/worldgen/processor_list/mossify_{20,70,10}_percent.json, verbatim. */
        public fun mossify(probability: Float): StructureProcessorList =
            StructureProcessorList(
                listOf(
                    StructureProcessorRule(
                        StructureRuleTest.RandomBlockMatch(blockId("minecraft:cobblestone"), probability),
                        null,
                        blockId("minecraft:mossy_cobblestone"),
                    ),
                ),
            )
    }
}
