package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * Single-function block families with no private helpers of their own, kept together to avoid a
 * dozen near-empty files. Each still says which vanilla class it came from.
 */
public object MiscGrowth {
    /** parity: `NyliumBlock.randomTick`: nylium under something opaque goes back to netherrack. */
    public fun nylium(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val above = world.state.getBlockState(x, y + 1, z) ?: return
        if (BlockShapes.dampening(above) < FULL_DAMPENING) return
        set(world, x, y, z, Blocks.NETHERRACK.stateId, changes)
    }

    /** parity: `RedStoneOreBlock.randomTick`, which is only ever the light going out. */
    public fun redstoneOre(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        if (BlockStateDefinitions.propertiesOf(stateId)["lit"] != "true") return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val dark = BlockStateDefinitions.stateIdOf(block, mapOf("lit" to "false"), stateId) ?: return
        set(world, x, y, z, dark, changes)
    }

    /** parity: `SweetBerryBushBlock.randomTick`: light nine above it and one roll in five. */
    public fun sweetBerries(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(BERRY_ODDS) != 0) return
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GROWTH_LIGHT) return
        grow(world, x, y, z, stateId, BERRY_MAX_AGE, changes)
    }

    /** parity: `BuddingAmethystBlock.randomTick`: one roll in five, one of the six faces. */
    public fun buddingAmethyst(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(AMETHYST_ODDS) != 0) return
        val facing = FACE_NAMES[random.nextInt(FACE_NAMES.size)]
        val targetX = x + offsetX(facing)
        val targetY = y + offsetY(facing)
        val targetZ = z + offsetZ(facing)
        val target = world.state.getBlockState(targetX, targetY, targetZ) ?: return
        val path = BlockStateDefinitions.blockOf(target)?.path
        val facingNow = BlockStateDefinitions.propertiesOf(target)["facing"]
        val next =
            when {
                target == Blocks.AIR.stateId || BlockSupport.holdsWater(target) -> "small_amethyst_bud"
                path == "small_amethyst_bud" && facingNow == facing -> "medium_amethyst_bud"
                path == "medium_amethyst_bud" && facingNow == facing -> "large_amethyst_bud"
                path == "large_amethyst_bud" && facingNow == facing -> "amethyst_cluster"
                else -> return
            }
        val grown =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla(next),
                mapOf("facing" to facing, "waterlogged" to BlockSupport.holdsWater(target).toString()),
            ) ?: return
        set(world, targetX, targetY, targetZ, grown, changes)
    }

    /**
     * parity: `TurtleEggBlock.randomTick`, minus the turtles: the egg cracks its two stages and
     * then goes, and hatching a turtle waits for entities. The odds are the default of
     * the `turtle_egg_hatch_chance` attribute, which is what an unmodified world gives every block.
     */
    public fun turtleEgg(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        // parity: `TurtleEggBlock.shouldUpdateHatchLevel` over the `turtle_egg_hatch_chance`
        // environment attribute, whose default is this. The attribute system itself is not
        // modelled, so the default is what every position gets.
        if (random.nextFloat() >= TURTLE_HATCH_CHANCE) return
        val below = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y - 1, z) ?: 0)?.path
        if (below != "sand" && below != "red_sand" && below != "suspicious_sand") return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val hatch = BlockStateDefinitions.propertiesOf(stateId)["hatch"]?.toIntOrNull() ?: return
        if (hatch < TURTLE_HATCH) {
            val cracked =
                BlockStateDefinitions.stateIdOf(block, mapOf("hatch" to (hatch + 1).toString()), stateId) ?: return
            set(world, x, y, z, cracked, changes)
            return
        }
        set(world, x, y, z, Blocks.AIR.stateId, changes)
    }

    /**
     * parity: `DriedGhastBlock.randomTick`, which does nothing itself but ask to be looked at again
     * in five thousand ticks; the drying and soaking happen on that scheduled turn.
     */
    public fun driedGhast(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val hydration = properties["hydration"]?.toIntOrNull() ?: 0
        if (properties["waterlogged"] != "true" && hydration <= 0) return
        world.state.scheduleBlockTick(x, y, z, ScheduledBlocks.DRIED_GHAST, DRIED_GHAST_DELAY)
    }

    /**
     * parity: `EyeblossomBlock.randomTick` over the `gameplay/eyeblossom_open` attribute, whose
     * only source in vanilla's data is the `day` timeline: true from tick 12600, false from 23401.
     *
     * The attribute system itself is not modelled, but this attribute has exactly one track and it
     * is one this build already keeps: the world clock.
     */
    public fun eyeblossom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        changes: MutableList<BlockChange>,
    ) {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return
        val open = path.startsWith("open_") || path.startsWith("potted_open_")
        if (open == eyeblossomsAreOpen(world)) return
        val other =
            when (path) {
                "open_eyeblossom" -> "closed_eyeblossom"
                "closed_eyeblossom" -> "open_eyeblossom"
                "potted_open_eyeblossom" -> "potted_closed_eyeblossom"
                "potted_closed_eyeblossom" -> "potted_open_eyeblossom"
                else -> return
            }
        val turned =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla(other), BlockStateDefinitions.propertiesOf(stateId))
                ?: Blocks.REGISTRY.idOf(Identifier.vanilla(other))
                ?: return
        set(world, x, y, z, turned, changes)
    }

    /** parity: the `day` timeline's two keyframes for this attribute. */
    private fun eyeblossomsAreOpen(world: LoadedWorld): Boolean {
        val time = ((world.clock.totalTicks % DAY_TICKS) + DAY_TICKS) % DAY_TICKS
        return time >= EYEBLOSSOM_OPENS && time < EYEBLOSSOM_CLOSES
    }

    private const val FULL_DAMPENING = 15
    private const val BERRY_ODDS = 5
    private const val BERRY_MAX_AGE = 3
    private const val AMETHYST_ODDS = 5
    private const val TURTLE_HATCH = 2

    /** parity: `EnvironmentAttributes.TURTLE_EGG_HATCH_CHANCE`'s default. */
    private const val TURTLE_HATCH_CHANCE = 0.002f

    /** parity: `DriedGhastBlock.randomTick`'s own delay. */
    private const val DRIED_GHAST_DELAY = 5000

    /** parity: the `day` timeline's keyframes for `gameplay/eyeblossom_open`. */
    private const val EYEBLOSSOM_OPENS = 12600L
    private const val EYEBLOSSOM_CLOSES = 23401L
    private const val DAY_TICKS = 24000L
}
