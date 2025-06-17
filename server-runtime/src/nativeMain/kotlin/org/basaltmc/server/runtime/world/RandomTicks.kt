package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** A block a random tick changed, so the caller can tell the clients about it. */
public class BlockChange(
    public val x: Int,
    public val y: Int,
    public val z: Int,
    public val stateId: Int,
)

/**
 * The blocks vanilla picks out of the air each tick and lets act.
 *
 * parity: `ServerLevel.tickChunk`'s `tickBlocks` half. Which positions come up is not a matter of
 * taste: the same `randValue * 3 + 1013904223` walk decides it, and how many sections it steps over
 * is part of that walk.
 *
 * What each block then does lives in [act]. A state with no case there does nothing.
 */
public class RandomTicks {
    /**
     * parity: `Level.randValue`, one walk per level, kept between ticks.
     *
     * Vanilla seeds it from a fresh random and this seeds it from the world's own, so the same
     * world picks the same positions every run: deterministic where vanilla is merely arbitrary.
     */
    private val walks = HashMap<String, Int>()

    /**
     * Runs one tick's worth over [chunks], returning what changed.
     *
     * parity: `randomTickSpeed` positions per section of each chunk near a player, and the section
     * is skipped altogether when nothing in it ticks.
     */
    public fun tick(
        world: LoadedWorld,
        chunks: List<IntArray>,
        speed: Int,
        random: LegacyRandom,
    ): List<BlockChange> {
        if (speed <= 0) return emptyList()
        val changes = mutableListOf<BlockChange>()
        for (position in chunks) {
            val chunk = world.state.chunkIfLoaded(position[0], position[1]) ?: continue
            val minX = position[0] shl 4
            val minZ = position[1] shl 4
            for (index in 0 until chunk.sectionCount) {
                // parity: `LevelChunkSection.isRandomlyTicking`. Skipping the sections that hold
                // nothing is not only cheaper: the walk below is shared, so how many sections are
                // visited is part of which positions come up at all.
                if (!chunk.section(index).mayContain { BlockSupport.ticksRandomly(it) }) continue
                val sectionY = chunk.verticalBounds.minY + (index shl 4)
                repeat(speed) {
                    val walk = nextWalk(world)
                    val x = minX + (walk and 15)
                    val y = sectionY + ((walk shr 16) and 15)
                    val z = minZ + ((walk shr 8) and 15)
                    val stateId = world.state.getBlockState(x, y, z)
                    if (stateId != null && BlockSupport.ticksRandomly(stateId)) {
                        act(world, x, y, z, stateId, random, changes)
                    }
                }
            }
        }
        return changes
    }

    /**
     * One position's worth, for a caller that already knows where: what [tick] does once it has
     * picked. A state that ticks at random and has no case here answers with nothing.
     */
    public fun tickBlock(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
    ): List<BlockChange> {
        val stateId = world.state.getBlockState(x, y, z) ?: return emptyList()
        if (!BlockSupport.ticksRandomly(stateId)) return emptyList()
        val changes = mutableListOf<BlockChange>()
        act(world, x, y, z, stateId, random, changes)
        return changes
    }

    /** parity: `Level.getBlockRandomPos`, whose value is what picks the position. */
    private fun nextWalk(world: LoadedWorld): Int {
        val next = (walks.getOrElse(world.name) { world.metadata.seed.toInt() } * 3) + 1013904223
        walks[world.name] = next
        return next shr 2
    }

    private fun act(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        when {
            block.path in CROPS -> growCrop(world, x, y, z, stateId, block, random, changes)
            block.path == "nether_wart" -> growNetherWart(world, x, y, z, stateId, block, random, changes)
            block.path == "sugar_cane" -> growStalk(world, x, y, z, stateId, block, changes)
            block.path == "ice" -> melt(world, x, y, z, stateId, Blocks.WATER.stateId, changes)
            block.path == "snow" -> melt(world, x, y, z, stateId, Blocks.AIR.stateId, changes)
        }
    }

    /** parity: `CropBlock.randomTick`: light nine or better, then a roll against the growth speed. */
    private fun growCrop(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (WorldLight.rawBrightness(world, x, y, z) < CROP_LIGHT) return
        val definition = BlockStateDefinitions.of(block) ?: return
        val ages = definition.valuesOf("age")
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= ages.size - 1) return
        val speed = growthSpeed(world, x, y, z, block)
        if (random.nextInt((25f / speed).toInt() + 1) != 0) return
        val grown = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
        set(world, x, y, z, grown, changes)
    }

    /** parity: `CropBlock.getGrowthSpeed`, farmland underneath and the neighbours' rows and all. */
    private fun growthSpeed(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        block: Identifier,
    ): Float {
        var speed = 1f
        for (dx in -1..1) {
            for (dz in -1..1) {
                val under = world.state.getBlockState(x + dx, y - 1, z + dz) ?: continue
                var here = 0f
                if (BlockStateDefinitions.blockOf(under)?.path == "farmland") {
                    here = 1f
                    if ((BlockStateDefinitions.propertiesOf(under)["moisture"]?.toIntOrNull() ?: 0) > 0) here = 3f
                }
                if (dx != 0 || dz != 0) here /= 4f
                speed += here
            }
        }
        val sameAt = { ax: Int, az: Int -> BlockStateDefinitions.blockOf(stateAt(world, ax, y, az)) == block }
        val horizontal = sameAt(x - 1, z) || sameAt(x + 1, z)
        val vertical = sameAt(x, z - 1) || sameAt(x, z + 1)
        if (horizontal && vertical) return speed / 2f
        val diagonal = sameAt(x - 1, z - 1) || sameAt(x + 1, z - 1) || sameAt(x + 1, z + 1) || sameAt(x - 1, z + 1)
        return if (diagonal) speed / 2f else speed
    }

    /** parity: `NetherWartBlock.randomTick`, which asks for no light at all and rolls one in ten. */
    private fun growNetherWart(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= NETHER_WART_MAX_AGE || random.nextInt(NETHER_WART_ODDS) != 0) return
        val grown = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
        set(world, x, y, z, grown, changes)
    }

    /**
     * parity: `SugarCaneBlock.randomTick`: count the stalk under it, and at age fifteen put another
     * block on top and start again from zero.
     *
     * A cactus grows by nearly the same rule and is deliberately not here: it also flowers at age
     * eight and keeps ageing past its third block, and half of that is worse than none of it.
     */
    private fun growStalk(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        changes: MutableList<BlockChange>,
    ) {
        if (stateAt(world, x, y + 1, z) != Blocks.AIR.stateId) return
        var height = 1
        while (BlockStateDefinitions.blockOf(stateAt(world, x, y - height, z)) == block) height++
        if (height >= MAX_STALK_HEIGHT) return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age < STALK_MAX_AGE) {
            val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
            set(world, x, y, z, older, changes)
            return
        }
        val fresh = BlockStateDefinitions.stateIdOf(block, mapOf("age" to "0"), stateId) ?: return
        set(world, x, y + 1, z, fresh, changes)
        set(world, x, y, z, fresh, changes)
    }

    /** parity: `IceBlock` and `SnowLayerBlock`, both of which go by the light the blocks give off. */
    private fun melt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        becomes: Int,
        changes: MutableList<BlockChange>,
    ) {
        if (WorldLight.blockLight(world, x, y, z) <= MELT_LIGHT - BlockShapes.dampening(stateId)) return
        set(world, x, y, z, becomes, changes)
    }

    private fun stateAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId

    private fun set(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        if (!world.state.setBlockState(x, y, z, stateId)) return
        changes += BlockChange(x, y, z, stateId)
    }

    private companion object {
        /** The blocks that grow by `CropBlock`'s own rule. Melons and pumpkins bring their stems later. */
        val CROPS = setOf("wheat", "carrots", "potatoes", "beetroots", "torchflower_crop")
        const val CROP_LIGHT = 9
        const val NETHER_WART_MAX_AGE = 3
        const val NETHER_WART_ODDS = 10
        const val STALK_MAX_AGE = 15
        const val MAX_STALK_HEIGHT = 3

        /** `11 - state.getLightDampening()` for ice, and a plain 11 for a snow layer, which dampens none. */
        const val MELT_LIGHT = 11
    }
}
