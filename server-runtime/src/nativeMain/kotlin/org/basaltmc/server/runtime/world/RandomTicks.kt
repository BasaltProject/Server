package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Weathering
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

    /** How a block that spawns something gets an id for it, when the caller can give one. */
    private var allocateEntityId: (() -> Int)? = null

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
        allocateEntityId: (() -> Int)? = null,
    ): List<BlockChange> {
        this.allocateEntityId = allocateEntityId
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
        allocateEntityId: (() -> Int)? = null,
    ): List<BlockChange> {
        this.allocateEntityId = allocateEntityId ?: this.allocateEntityId
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
            block.path == "cactus" -> growCactus(world, x, y, z, stateId, block, random, changes)
            BlockConnections.isLeaves(stateId) -> decay(world, x, y, z, stateId, changes)
            SaplingGrowth.isSapling(stateId) -> growSapling(world, x, y, z, stateId, random, changes)
            // parity: `SpreadingSnowyBlock`, whose two blocks are grass and mycelium. Podzol is a
            // plain `SnowyBlock` and does not tick at all.
            block.path == "grass_block" || block.path == "mycelium" ->
                spreadGrass(world, x, y, z, stateId, random, changes)
            block.path == "farmland" -> dryFarmland(world, x, y, z, stateId, changes)
            block.path == "crimson_nylium" || block.path == "warped_nylium" ->
                BlockGrowth.nylium(world, x, y, z, changes)
            block.path == "redstone_ore" || block.path == "deepslate_redstone_ore" ->
                BlockGrowth.redstoneOre(world, x, y, z, stateId, changes)
            block.path == "cocoa" -> BlockGrowth.cocoa(world, x, y, z, stateId, random, changes)
            block.path == "sweet_berry_bush" -> BlockGrowth.sweetBerries(world, x, y, z, stateId, random, changes)
            block.path == "brown_mushroom" || block.path == "red_mushroom" ->
                BlockGrowth.mushroom(world, x, y, z, stateId, random, changes)
            block.path == "pumpkin_stem" || block.path == "melon_stem" ->
                BlockGrowth.stem(world, x, y, z, stateId, growthSpeed(world, x, y, z, block), random, changes)
            block.path == "bamboo" -> BlockGrowth.bamboo(world, x, y, z, stateId, random, changes)
            block.path == "bamboo_sapling" -> BlockGrowth.bambooSapling(world, x, y, z, random, changes)
            block.path == "nether_portal" ->
                allocateEntityId?.let { BlockGrowth.netherPortal(world, x, y, z, random, it) }
            block.path.endsWith("eyeblossom") -> BlockGrowth.eyeblossom(world, x, y, z, stateId, changes)
            block.path == "pointed_dripstone" -> BlockGrowth.dripstone(world, x, y, z, stateId, random, changes)
            block.path == "vine" -> BlockGrowth.vine(world, x, y, z, stateId, random, changes)
            block.path == "dried_ghast" -> BlockGrowth.driedGhast(world, x, y, z, stateId)
            block.path == "chorus_flower" -> BlockGrowth.chorusFlower(world, x, y, z, stateId, random, changes)
            block.path == "budding_amethyst" -> BlockGrowth.buddingAmethyst(world, x, y, z, random, changes)
            block.path == "turtle_egg" -> BlockGrowth.turtleEgg(world, x, y, z, stateId, random, changes)
            block.path == "lava" -> BlockGrowth.lava(world, x, y, z, random, changes)
            block.path in CLIMBING_PLANTS ->
                BlockGrowth.growingPlant(
                    world,
                    x,
                    y,
                    z,
                    stateId,
                    upwards = block.path == "kelp" || block.path == "twisting_vines",
                    chance = if (block.path == "kelp") KELP_CHANCE else VINE_CHANCE,
                    random = random,
                    changes = changes,
                )
            Weathering.weathers(block.path) -> BlockGrowth.weather(world, x, y, z, stateId, random, changes)
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
        // parity: `BeetrootBlock.randomTick`, which throws away two turns in three before asking
        // its parent anything. Without this a beetroot grows three times as fast as vanilla's.
        if (block.path == "beetroots" && random.nextInt(BEETROOT_ODDS) == 0) return
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

    /** parity: `SaplingBlock.randomTick`: light nine above it, then one roll in seven. */
    private fun growSapling(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (WorldLight.rawBrightness(world, x, y + 1, z) < SaplingGrowth.GROWTH_LIGHT) return
        if (random.nextInt(SaplingGrowth.GROWTH_ODDS) != 0) return
        changes += SaplingGrowth.advance(world, x, y, z, stateId, random)
    }

    /**
     * parity: `CactusBlock.randomTick`, which is the same shape as sugar cane with two twists: at
     * age eight it may flower instead, and a stalk that has reached three keeps ageing rather than
     * stopping, so a cactus at fifteen and three tall does nothing at all.
     */
    private fun growCactus(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (stateAt(world, x, y + 1, z) != Blocks.AIR.stateId) return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        var height = 1
        while (BlockStateDefinitions.blockOf(stateAt(world, x, y - height, z)) == block) {
            if (++height == MAX_STALK_HEIGHT && age == STALK_MAX_AGE) return
        }
        if (age == FLOWER_AGE && NeighbourUpdates.isSupported(world, x, y + 1, z, stateId)) {
            val odds = if (height >= MAX_STALK_HEIGHT) TALL_FLOWER_ODDS else SHORT_FLOWER_ODDS
            if (random.nextDouble() <= odds) {
                val flower = Blocks.REGISTRY.idOf(Identifier.vanilla("cactus_flower"))
                flower?.let { set(world, x, y + 1, z, it, changes) }
            }
        } else if (age == STALK_MAX_AGE && height < MAX_STALK_HEIGHT) {
            val fresh = BlockStateDefinitions.stateIdOf(block, mapOf("age" to "0"), stateId) ?: return
            set(world, x, y + 1, z, fresh, changes)
            set(world, x, y, z, fresh, changes)
            return
        }
        if (age < STALK_MAX_AGE) {
            val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
            set(world, x, y, z, older, changes)
        }
    }

    /**
     * parity: `SpreadingSnowyBlock.randomTick`: grass under something opaque turns back to dirt,
     * and otherwise tries four times to spread onto a nearby dirt block.
     *
     * The four attempts are four rolls of the world's random whether they land anywhere or not,
     * which is what keeps a field of grass spreading at vanilla's own pace.
     */
    private fun spreadGrass(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (!staysGrass(world, x, y, z)) {
            set(world, x, y, z, Blocks.DIRT.stateId, changes)
            return
        }
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GRASS_LIGHT) return
        repeat(GRASS_ATTEMPTS) {
            val tx = x + random.nextInt(3) - 1
            val ty = y + random.nextInt(5) - 3
            val tz = z + random.nextInt(3) - 1
            // parity: both spread onto plain dirt, which is `SpreadingSnowyBlock`'s base block.
            if (stateAt(world, tx, ty, tz) != Blocks.DIRT.stateId) return@repeat
            if (!staysGrass(world, tx, ty, tz)) return@repeat
            if (BlockSupport.holdsWater(stateAt(world, tx, ty + 1, tz))) return@repeat
            set(world, tx, ty, tz, stateId, changes)
        }
    }

    /**
     * parity: `SpreadingSnowyBlock.canStayAlive`: a single snow layer is fine, a full fluid is not,
     * and anything else has to let light through.
     */
    private fun staysGrass(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val above = stateAt(world, x, y + 1, z)
        val properties = BlockStateDefinitions.propertiesOf(above)
        if (BlockStateDefinitions.blockOf(above)?.path == "snow" && properties["layers"] == "1") return true
        if (BlockSupport.holdsWater(above)) return false
        return BlockShapes.dampening(above) < WorldLight.MAX_LEVEL
    }

    /**
     * parity: `FarmlandBlock.randomTick`: away from water it dries a step at a time, and at zero it
     * goes back to dirt unless something growing on it holds it.
     */
    private fun dryFarmland(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val moisture = BlockStateDefinitions.propertiesOf(stateId)["moisture"]?.toIntOrNull() ?: return
        if (nearWater(world, x, y, z) || world.weather.raining) {
            if (moisture >= MAX_MOISTURE) return
            val wet = BlockStateDefinitions.stateIdOf(block, mapOf("moisture" to MAX_MOISTURE.toString()), stateId)
            wet?.let { set(world, x, y, z, it, changes) }
            return
        }
        if (moisture > 0) {
            val drier = BlockStateDefinitions.stateIdOf(block, mapOf("moisture" to (moisture - 1).toString()), stateId)
            drier?.let { set(world, x, y, z, it, changes) }
            return
        }
        if (BlockConnections.maintainsFarmland(stateAt(world, x, y + 1, z))) return
        set(world, x, y, z, Blocks.DIRT.stateId, changes)
    }

    /** parity: `FarmlandBlock.isNearWater`, four blocks out and one up. */
    private fun nearWater(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        for (dx in -WATER_RANGE..WATER_RANGE) {
            for (dz in -WATER_RANGE..WATER_RANGE) {
                for (dy in 0..1) {
                    if (BlockSupport.holdsWater(stateAt(world, x + dx, y + dy, z + dz))) return true
                }
            }
        }
        return false
    }

    /**
     * parity: `LeavesBlock.randomTick`, which drops leaves that are seven blocks from any trunk.
     *
     * A canopy still on its trunk is within seven blocks of it and stays; one that has been felled
     * is not, and goes.
     */
    private fun decay(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        // parity: `LeavesBlock.decaying`, which only reads the state. The stored distance is right
        // because `LeafDistances` stamps it when the chunk arrives and `updateShape` keeps it so.
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["persistent"] == "true") return
        if ((properties["distance"]?.toIntOrNull() ?: DECAY_DISTANCE) < DECAY_DISTANCE) return
        set(world, x, y, z, Blocks.AIR.stateId, changes)
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

        /** parity: `BeetrootBlock.randomTick`'s `nextInt(3) != 0`. */
        const val BEETROOT_ODDS = 3
        const val NETHER_WART_MAX_AGE = 3
        const val NETHER_WART_ODDS = 10
        const val STALK_MAX_AGE = 15
        const val MAX_STALK_HEIGHT = 3

        /** `11 - state.getLightDampening()` for ice, and a plain 11 for a snow layer, which dampens none. */
        const val MELT_LIGHT = 11

        /** parity: the four `GrowingPlantHeadBlock` subclasses and their own odds. */
        val CLIMBING_PLANTS = setOf("kelp", "cave_vines", "twisting_vines", "weeping_vines")
        const val KELP_CHANCE = 0.14
        const val VINE_CHANCE = 0.1

        /** parity: `LeavesBlock.DECAY_DISTANCE`. */
        const val DECAY_DISTANCE = 7

        const val GRASS_LIGHT = 9
        const val GRASS_ATTEMPTS = 4
        const val MAX_MOISTURE = 7

        /** parity: `FarmlandBlock.isNearWater`'s own four. */
        const val WATER_RANGE = 4

        /** parity: `CactusBlock`, which flowers at age eight and more often once it is tall. */
        const val FLOWER_AGE = 8
        const val SHORT_FLOWER_ODDS = 0.1
        const val TALL_FLOWER_ODDS = 0.25
    }
}
