package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.PositionalHashing
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * // parity: `templatesystem.StructureProcessor`. A template's blocks pass through these in order and
 * any may drop a block, which is why this is not [StructureProcessorList].
 */
public fun interface TemplateBlockProcessor {
    /**
     * The state to place at [position], or null to place nothing there at all.
     * [worldStateId] is what the world already holds, which several of these consult.
     */
    public fun process(
        inputStateId: Int,
        worldStateId: Int,
        position: StructurePos,
    ): Int?
}

/** Applies [processors] in order, stopping the moment one drops the block. */
public class TemplateBlockProcessorChain(
    private val processors: List<TemplateBlockProcessor>,
) : TemplateBlockProcessor {
    override fun process(
        inputStateId: Int,
        worldStateId: Int,
        position: StructurePos,
    ): Int? {
        var state = inputStateId
        for (processor in processors) {
            state = processor.process(state, worldStateId, position) ?: return null
        }
        return state
    }
}

public object TemplateBlockProcessors {
    /** // parity: BlockIgnoreProcessor.STRUCTURE_BLOCK, which drops only the structure void. */
    public val IGNORE_STRUCTURE_BLOCK: TemplateBlockProcessor =
        TemplateBlockProcessor { input, _, _ -> input.takeUnless { path(it) in STRUCTURE_ONLY } }

    /** // parity: BlockIgnoreProcessor.STRUCTURE_AND_AIR, which drops its air too. */
    public val IGNORE_STRUCTURE_AND_AIR: TemplateBlockProcessor =
        TemplateBlockProcessor { input, _, _ ->
            input.takeUnless { path(it) in STRUCTURE_ONLY || Blocks.isAir(it) }
        }

    /**
     * // parity: ProtectedBlockProcessor. Whatever the world already put here and marked as not
     * replaceable by features stays: a structure never eats another one's blocks.
     */
    public val PROTECT_FEATURES: TemplateBlockProcessor =
        TemplateBlockProcessor { input, world, _ ->
            input.takeUnless { path(world) in FEATURES_CANNOT_REPLACE }
        }

    /** // parity: LavaSubmergedBlockProcessor: anything not a full cube keeps the lava it sits in. */
    public val LAVA_SUBMERGED: TemplateBlockProcessor =
        TemplateBlockProcessor { input, world, _ ->
            // parity: `Block.isShapeFullBlock`, a full cube and not merely something solid: a slab
            // or a stair keeps the lava around it, a netherrack block displaces it.
            if (path(world) == "lava" && !BlockShapes.isSolidRender(input)) LAVA else input
        }

    /** // parity: BlackstoneReplaceProcessor, the Nether variant's stone swap. */
    public val BLACKSTONE_REPLACE: TemplateBlockProcessor =
        TemplateBlockProcessor { input, _, _ ->
            val replacement = BLACKSTONE_BY_PATH[path(input)] ?: return@TemplateBlockProcessor input
            keepProperties(input, replacement)
        }

    /**
     * // parity: BlockAgeProcessor. The draw order is not the obvious one: building the replacement arrays
     * calls `getRandomFacingStairs` twice, so four draws happen before anything is chosen.
     */
    public fun age(mossiness: Float): TemplateBlockProcessor =
        TemplateBlockProcessor { input, _, position ->
            val random = WorldgenRandom(LegacyRandom(PositionalHashing.getSeed(position.x, position.y, position.z)))
            val path = path(input)
            when {
                path == "stone_bricks" || path == "stone" || path == "chiseled_stone_bricks" ->
                    fullStoneBlock(random, mossiness) ?: input
                path?.endsWith("_stairs") == true -> stairs(input, random, mossiness) ?: input
                path?.endsWith("_slab") == true ->
                    if (random.nextFloat() < mossiness) keepProperties(input, "mossy_stone_brick_slab") else input
                path?.endsWith("_wall") == true ->
                    if (random.nextFloat() < mossiness) keepProperties(input, "mossy_stone_brick_wall") else input
                path == "obsidian" ->
                    if (random.nextFloat() < OBSIDIAN_CRYING_CHANCE) state("crying_obsidian") else input
                else -> input
            }
        }

    private fun fullStoneBlock(
        random: WorldgenRandom,
        mossiness: Float,
    ): Int? {
        if (random.nextFloat() >= REPLACE_FULL_BLOCK_CHANCE) return null
        val nonMossy = intArrayOf(state("cracked_stone_bricks"), randomFacingStairs(random, "stone_brick_stairs"))
        val mossy = intArrayOf(state("mossy_stone_bricks"), randomFacingStairs(random, "mossy_stone_brick_stairs"))
        return pick(random, mossiness, nonMossy, mossy)
    }

    private fun stairs(
        input: Int,
        random: WorldgenRandom,
        mossiness: Float,
    ): Int? {
        if (random.nextFloat() >= REPLACE_STAIRS_CHANCE) return null
        val mossy = intArrayOf(keepProperties(input, "mossy_stone_brick_stairs"), state("mossy_stone_brick_slab"))
        return pick(random, mossiness, NON_MOSSY_REPLACEMENTS, mossy)
    }

    private fun pick(
        random: WorldgenRandom,
        mossiness: Float,
        nonMossy: IntArray,
        mossy: IntArray,
    ): Int {
        val chosen = if (random.nextFloat() < mossiness) mossy else nonMossy
        return chosen[random.nextInt(chosen.size)]
    }

    private fun randomFacingStairs(
        random: WorldgenRandom,
        path: String,
    ): Int {
        val facing = HORIZONTAL[random.nextInt(HORIZONTAL.size)]
        val half = HALVES[random.nextInt(HALVES.size)]
        return Blocks.stateIdOf(Identifier.vanilla(path), mapOf("facing" to facing, "half" to half)) ?: state(path)
    }

    /** // parity: `withPropertiesOf`, which carries over every property the two blocks share. */
    private fun keepProperties(
        input: Int,
        path: String,
    ): Int {
        val properties = Blocks.byStateId(input)?.properties ?: return state(path)
        return Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: state(path)
    }

    private fun path(stateId: Int): String? = Blocks.byStateId(stateId)?.block?.path

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    private const val REPLACE_FULL_BLOCK_CHANCE = 0.5f
    private const val REPLACE_STAIRS_CHANCE = 0.5f
    private const val OBSIDIAN_CRYING_CHANCE = 0.15f

    private val LAVA = state("lava")
    private val HORIZONTAL = listOf("north", "south", "west", "east")
    private val HALVES = listOf("bottom", "top")
    private val NON_MOSSY_REPLACEMENTS = intArrayOf(state("stone_slab"), state("stone_brick_slab"))
    private val STRUCTURE_ONLY = setOf("structure_void", "structure_block", "jigsaw")
    private val BLACKSTONE_BY_PATH =
        mapOf(
            "cobblestone" to "blackstone",
            "mossy_cobblestone" to "blackstone",
            "stone" to "blackstone",
            "cobblestone_stairs" to "blackstone_stairs",
            "stone_stairs" to "blackstone_stairs",
            "mossy_cobblestone_stairs" to "blackstone_stairs",
            "cobblestone_slab" to "blackstone_slab",
            "stone_slab" to "blackstone_slab",
            "mossy_cobblestone_slab" to "blackstone_slab",
            "cobblestone_wall" to "blackstone_wall",
            "mossy_cobblestone_wall" to "blackstone_wall",
        )

    /** The blocks a feature is never allowed to overwrite. // parity: `#features_cannot_replace`. */
    private val FEATURES_CANNOT_REPLACE =
        setOf(
            "bedrock",
            "spawner",
            "chest",
            "end_portal_frame",
            "reinforced_deepslate",
            "trial_spawner",
            "vault",
        )
}
