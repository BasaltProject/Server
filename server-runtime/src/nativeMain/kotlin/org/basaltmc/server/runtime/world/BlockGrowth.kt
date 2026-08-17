package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.FireOdds
import org.basaltmc.server.registry.Weathering
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The rest of what vanilla grows, spreads and puts out on a random tick.
 *
 * One function per `randomTick` in the jar, kept apart from [RandomTicks] only because that file is
 * the picker and this one is the behaviour. Each says which class it came from.
 */
@OptIn(ExperimentalUuidApi::class)
public object BlockGrowth {
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

    /** parity: `CocoaBlock.randomTick`: one roll in five, three ages. */
    public fun cocoa(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(COCOA_ODDS) != 0) return
        grow(world, x, y, z, stateId, COCOA_MAX_AGE, changes)
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

    /**
     * parity: `MushroomBlock.randomTick`: one roll in twenty-five, no more than five of its kind in
     * a nine by three by nine box, and a walk of four hops to find somewhere dark enough to sit.
     */
    public fun mushroom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(MUSHROOM_ODDS) != 0) return
        var room = MUSHROOM_CROWD
        for (dx in -4..4) {
            for (dy in -1..1) {
                for (dz in -4..4) {
                    if (world.state.getBlockState(x + dx, y + dy, z + dz) != stateId) continue
                    if (--room <= 0) return
                }
            }
        }
        var hopX = x + random.nextInt(3) - 1
        var hopY = y + random.nextInt(2) - random.nextInt(2)
        var hopZ = z + random.nextInt(3) - 1
        var fromX = x
        var fromY = y
        var fromZ = z
        repeat(4) {
            if (fits(world, hopX, hopY, hopZ, stateId)) {
                fromX = hopX
                fromY = hopY
                fromZ = hopZ
            }
            hopX = fromX + random.nextInt(3) - 1
            hopY = fromY + random.nextInt(2) - random.nextInt(2)
            hopZ = fromZ + random.nextInt(3) - 1
        }
        if (fits(world, hopX, hopY, hopZ, stateId)) set(world, hopX, hopY, hopZ, stateId, changes)
    }

    /**
     * parity: `StemBlock.randomTick`: the same growth speed a crop has, and at full age a fruit on
     * one of the four sides with the stem turned to face it.
     */
    public fun stem(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        speed: Float,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (WorldLight.rawBrightness(world, x, y, z) < GROWTH_LIGHT) return
        if (random.nextInt((25f / speed).toInt() + 1) != 0) return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age < STEM_MAX_AGE) {
            val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
            set(world, x, y, z, older, changes)
            return
        }
        val facing = HORIZONTAL_NAMES[random.nextInt(HORIZONTAL_NAMES.size)]
        val fruitX = x + offsetX(facing)
        val fruitZ = z + offsetZ(facing)
        if (world.state.getBlockState(fruitX, y, fruitZ) != Blocks.AIR.stateId) return
        val ground = world.state.getBlockState(fruitX, y - 1, fruitZ) ?: return
        val groundPath = BlockStateDefinitions.blockOf(ground)?.path
        if (groundPath !in FRUIT_GROUND) return
        val fruit = FRUIT_OF[block.path] ?: return
        val attached = ATTACHED_OF[block.path] ?: return
        val fruitState = Blocks.REGISTRY.idOf(Identifier.vanilla(fruit)) ?: return
        val attachedState =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla(attached), mapOf("facing" to facing)) ?: return
        set(world, fruitX, y, fruitZ, fruitState, changes)
        set(world, x, y, z, attachedState, changes)
    }

    /**
     * parity: `BambooStalkBlock.randomTick` and its `growBamboo`: one roll in three, light nine, and
     * a stalk that carries its leaves down as it climbs.
     */
    public fun bamboo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["stage"] == "1") return
        if (random.nextInt(BAMBOO_ODDS) != 0) return
        if (world.state.getBlockState(x, y + 1, z) != Blocks.AIR.stateId) return
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GROWTH_LIGHT) return

        var height = 1
        while (isBamboo(world, x, y - height, z)) height++
        if (height >= BAMBOO_MAX_HEIGHT) return

        val below = world.state.getBlockState(x, y - 1, z) ?: return
        val twoBelow = world.state.getBlockState(x, y - 2, z) ?: return
        val belowLeaves = BlockStateDefinitions.propertiesOf(below)["leaves"]
        var leaves = "none"
        if (!isBamboo(world, x, y - 1, z) || belowLeaves == "none") {
            leaves = "small"
        } else {
            leaves = "large"
            if (isBamboo(world, x, y - 2, z)) {
                shrink(world, x, y - 1, z, below, "small", changes)
                shrink(world, x, y - 2, z, twoBelow, "none", changes)
            }
        }
        val age = if (properties["age"] != "1" && !isBamboo(world, x, y - 2, z)) "0" else "1"
        val stage =
            if ((height < BAMBOO_TALL || random.nextFloat() >= BAMBOO_STAGE_CHANCE) &&
                height != 15
            ) {
                "0"
            } else {
                "1"
            }
        val grown =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("bamboo"),
                mapOf("age" to age, "leaves" to leaves, "stage" to stage),
            ) ?: return
        set(world, x, y + 1, z, grown, changes)
    }

    /** parity: `BambooSaplingBlock.randomTick`, which puts a stalk above itself. */
    public fun bambooSapling(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(BAMBOO_ODDS) != 0) return
        if (world.state.getBlockState(x, y + 1, z) != Blocks.AIR.stateId) return
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GROWTH_LIGHT) return
        val stalk = Blocks.REGISTRY.idOf(Identifier.vanilla("bamboo")) ?: return
        set(world, x, y + 1, z, stalk, changes)
    }

    /**
     * parity: `ChangeOverTimeBlock.changeOverTime` and its `getNextState`: a block weathers about
     * once a day, more readily the more weathered its neighbours are, and never while one younger
     * than it stands within four blocks.
     */
    public fun weather(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextFloat() >= WEATHER_DAILY_CHANCE) return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val stage = Weathering.stageOf(block.path) ?: return
        var same = 0
        var older = 0
        for (dx in -4..4) {
            for (dy in -4..4) {
                for (dz in -4..4) {
                    if (dx == 0 && dy == 0 && dz == 0) continue
                    if (kotlin.math.abs(dx) + kotlin.math.abs(dy) + kotlin.math.abs(dz) > WEATHER_RANGE) continue
                    val neighbour = world.state.getBlockState(x + dx, y + dy, z + dz) ?: continue
                    val path = BlockStateDefinitions.blockOf(neighbour)?.path ?: continue
                    val theirs = Weathering.stageOf(path) ?: continue
                    if (theirs < stage) return
                    if (theirs > stage) older++ else same++
                }
            }
        }
        val share = (older + 1).toFloat() / (older + same + 1)
        if (random.nextFloat() >= share * share) return
        val next = Weathering.next(block.path) ?: return
        val weathered =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla(next),
                BlockStateDefinitions.propertiesOf(stateId),
            ) ?: Blocks.REGISTRY.idOf(Identifier.vanilla(next)) ?: return
        set(world, x, y, z, weathered, changes)
    }

    /**
     * parity: `GrowingPlantHeadBlock.randomTick`: kelp, cave vines and the nether vines all climb
     * their own way at their own odds until they reach age twenty-five.
     */
    public fun growingPlant(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        upwards: Boolean,
        chance: Double,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= PLANT_MAX_AGE) return
        if (random.nextDouble() >= chance) return
        val nextY = if (upwards) y + 1 else y - 1
        val ahead = world.state.getBlockState(x, nextY, z) ?: return
        // parity: `canGrowInto`, which for kelp is water and for the rest is air.
        val room = if (block.path == "kelp") BlockSupport.holdsWater(ahead) else ahead == Blocks.AIR.stateId
        if (!room) return
        val grown =
            BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
        set(world, x, nextY, z, grown, changes)
        val body = BODY_OF[block.path] ?: return
        Blocks.REGISTRY.idOf(Identifier.vanilla(body))?.let { set(world, x, y, z, it, changes) }
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
     * then goes, and hatching a turtle waits for the entities of §6.4. The odds are the default of
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
     * parity: `LavaFluid.randomTick`: lava sets light to the air above what will burn, and reaches
     * up to three blocks up when it has room.
     */
    public fun lava(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val passes = random.nextInt(3)
        if (passes > 0) {
            var testX = x
            var testY = y
            var testZ = z
            repeat(passes) {
                testX += random.nextInt(3) - 1
                testY += 1
                testZ += random.nextInt(3) - 1
                val here = world.state.getBlockState(testX, testY, testZ) ?: return
                if (here == Blocks.AIR.stateId) {
                    if (flammableAround(world, testX, testY, testZ)) {
                        light(world, testX, testY, testZ, changes)
                        return
                    }
                } else if (BlockShapes.blocksMotion(here)) {
                    return
                }
            }
            return
        }
        repeat(3) {
            val testX = x + random.nextInt(3) - 1
            val testZ = z + random.nextInt(3) - 1
            if (world.state.getBlockState(testX, y + 1, testZ) != Blocks.AIR.stateId) return@repeat
            if (igniteOddsAt(world, testX, y, testZ) <= 0) return@repeat
            light(world, testX, y + 1, testZ, changes)
        }
    }

    private fun light(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val fire = Blocks.REGISTRY.idOf(Identifier.vanilla("fire")) ?: return
        set(world, x, y, z, fire, changes)
    }

    private fun flammableAround(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        igniteOddsAt(world, x - 1, y, z) > 0 ||
            igniteOddsAt(world, x + 1, y, z) > 0 ||
            igniteOddsAt(world, x, y, z - 1) > 0 ||
            igniteOddsAt(world, x, y, z + 1) > 0 ||
            igniteOddsAt(world, x, y - 1, z) > 0 ||
            igniteOddsAt(world, x, y + 1, z) > 0

    private fun igniteOddsAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = FireOdds.igniteOdds(world.state.getBlockState(x, y, z) ?: 0)

    private fun offsetY(facing: String): Int =
        if (facing == "down") {
            -1
        } else if (facing == "up") {
            1
        } else {
            0
        }

    /**
     * parity: `ChorusFlowerBlock.randomTick`: it climbs while it has room, branches sideways when
     * it does not, and dies where it can do neither.
     */
    public fun chorusFlower(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (world.state.getBlockState(x, y + 1, z) != Blocks.AIR.stateId) return
        if (y + 1 > world.maxY) return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= CHORUS_DEAD_AGE) return

        var grows = false
        var onPillar = false
        val below = world.state.getBlockState(x, y - 1, z) ?: return
        val belowPath = BlockStateDefinitions.blockOf(below)?.path
        if (belowPath in CHORUS_GROUND) {
            grows = true
        } else if (belowPath == "chorus_plant") {
            var height = 1
            repeat(4) {
                val test = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y - height - 1, z) ?: 0)?.path
                if (test != "chorus_plant") {
                    if (test in CHORUS_GROUND) onPillar = true
                    return@repeat
                }
                height++
            }
            if (height < 2 || height <= random.nextInt(if (onPillar) 5 else 4)) grows = true
        } else if (below == Blocks.AIR.stateId) {
            grows = true
        }

        if (grows &&
            sidesEmpty(world, x, y + 1, z, null) &&
            world.state.getBlockState(x, y + 2, z) == Blocks.AIR.stateId
        ) {
            plant(world, x, y, z, changes)
            flower(world, x, y + 1, z, age, changes)
            return
        }
        if (age >= CHORUS_BRANCH_AGE) {
            flower(world, x, y, z, CHORUS_DEAD_AGE, changes)
            return
        }
        var attempts = random.nextInt(4)
        if (onPillar) attempts++
        var branched = false
        repeat(attempts) {
            val facing = HORIZONTAL_NAMES[random.nextInt(HORIZONTAL_NAMES.size)]
            val branchX = x + offsetX(facing)
            val branchZ = z + offsetZ(facing)
            if (world.state.getBlockState(branchX, y, branchZ) != Blocks.AIR.stateId) return@repeat
            if (world.state.getBlockState(branchX, y - 1, branchZ) != Blocks.AIR.stateId) return@repeat
            if (!sidesEmpty(world, branchX, y, branchZ, opposite(facing))) return@repeat
            flower(world, branchX, y, branchZ, age + 1, changes)
            branched = true
        }
        if (branched) plant(world, x, y, z, changes) else flower(world, x, y, z, CHORUS_DEAD_AGE, changes)
    }

    private fun flower(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        age: Int,
        changes: MutableList<BlockChange>,
    ) {
        val state =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla("chorus_flower"), mapOf("age" to age.toString()))
                ?: return
        set(world, x, y, z, state, changes)
    }

    private fun plant(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stem = Blocks.REGISTRY.idOf(Identifier.vanilla("chorus_plant")) ?: return
        set(world, x, y, z, stem, changes)
    }

    /** parity: `ChorusFlowerBlock.allNeighborsEmpty`, which lets one side through. */
    private fun sidesEmpty(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        ignore: String?,
    ): Boolean {
        for (facing in HORIZONTAL_NAMES) {
            if (facing == ignore) continue
            if (world.state.getBlockState(x + offsetX(facing), y, z + offsetZ(facing)) != Blocks.AIR.stateId) {
                return false
            }
        }
        return true
    }

    private fun opposite(facing: String): String =
        when (facing) {
            "north" -> "south"
            "south" -> "north"
            "west" -> "east"
            else -> "west"
        }

    /**
     * parity: `VineBlock.randomTick`, under the `spreadVines` game rule: one roll in four picks a
     * direction, and a vine only spreads while fewer than five of its kind stand within four blocks.
     *
     * Its full walk carries a vine round corners and up over ledges; what is here is the two moves
     * that make a wall grow over: taking a face beside it, and climbing to the block above.
     */
    public fun vine(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (!world.gameRules[GameRuleRegistry.SPREAD_VINES]) return
        if (random.nextInt(VINE_ODDS) != 0) return
        if (!vineHasRoom(world, x, y, z, stateId)) return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = FACE_NAMES[random.nextInt(FACE_NAMES.size)]

        if (facing == "up") {
            if (y >= world.maxY) return
            if (properties["up"] == "true") return
            if (!clingsTo(world, x, y + 1, z, "up")) return
            val climbed = BlockStateDefinitions.stateIdOf(block, mapOf("up" to "true"), stateId) ?: return
            set(world, x, y, z, climbed, changes)
            return
        }
        if (facing == "down") return
        if (properties[facing] == "true") return
        val sideX = x + offsetX(facing)
        val sideZ = z + offsetZ(facing)
        val beside = world.state.getBlockState(sideX, y, sideZ) ?: return
        if (beside != Blocks.AIR.stateId) {
            // Something to hold on to right there: take that face.
            if (!clingsTo(world, sideX, y, sideZ, facing)) return
            val held = BlockStateDefinitions.stateIdOf(block, mapOf(facing to "true"), stateId) ?: return
            set(world, x, y, z, held, changes)
        }
    }

    /** parity: `VineBlock.canSpread`, five of its kind in a nine by three by nine box. */
    private fun vineHasRoom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return false
        var room = VINE_CROWD
        for (dx in -4..4) {
            for (dy in -1..1) {
                for (dz in -4..4) {
                    val here = world.state.getBlockState(x + dx, y + dy, z + dz) ?: continue
                    if (BlockStateDefinitions.blockOf(here) != block) continue
                    if (--room <= 0) return false
                }
            }
        }
        return true
    }

    /** parity: `VineBlock.isAcceptableNeighbour` over `MultifaceBlock.canAttachTo`. */
    private fun clingsTo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        facing: String,
    ): Boolean {
        val neighbour = world.state.getBlockState(x, y, z) ?: return false
        val face =
            when (facing) {
                "up" -> org.basaltmc.server.registry.BlockFace.DOWN
                "north" -> org.basaltmc.server.registry.BlockFace.SOUTH
                "south" -> org.basaltmc.server.registry.BlockFace.NORTH
                "west" -> org.basaltmc.server.registry.BlockFace.EAST
                else -> org.basaltmc.server.registry.BlockFace.WEST
            }
        return BlockShapes.isFaceSturdy(neighbour, face)
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
     * parity: `SpeleothemBlock.randomTick` over `growStalactiteOrStalagmiteIfPossible`: a stalactite
     * hanging under dripstone grows a tip down, or drips a stalagmite up from the floor below it.
     *
     * What is not here is the other half of `PointedDripstoneBlock.randomTick`, which fills a
     * cauldron under it. Cauldrons are §6.5, and the block is what this system owns.
     */
    public fun dripstone(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextFloat() >= DRIPSTONE_CHANCE) return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["vertical_direction"] != "down") return
        // parity: `isStalactiteStartPos`, the topmost block of the stalactite.
        if (isDripstone(world, x, y + 1, z)) return
        // parity: `canGrow`, which asks for the block it hangs from, a dripstone block.
        if (BlockStateDefinitions.blockOf(world.state.getBlockState(x, y + 1, z) ?: 0)?.path !=
            "dripstone_block"
        ) {
            return
        }

        var tipY = y
        var found = false
        repeat(DRIPSTONE_REACH) {
            val here = world.state.getBlockState(x, tipY, z) ?: return
            val thickness = BlockStateDefinitions.propertiesOf(here)["thickness"]
            if (thickness == "tip") {
                found = true
                return@repeat
            }
            if (!isDripstone(world, x, tipY - 1, z)) return
            tipY--
        }
        if (!found) return
        val tip = world.state.getBlockState(x, tipY, z) ?: return
        val tipProperties = BlockStateDefinitions.propertiesOf(tip)
        // parity: `isFreeHangingStalactite`, which will not grow from a waterlogged tip.
        if (tipProperties["waterlogged"] == "true") return
        val below = world.state.getBlockState(x, tipY - 1, z) ?: return
        if (below != Blocks.AIR.stateId) return

        if (random.nextBoolean()) {
            placeTip(world, x, tipY - 1, z, "down", changes)
            return
        }
        // parity: `growStalagmiteBelow`, ten blocks down for a floor to grow up from.
        var floorY = tipY - 1
        repeat(DRIPSTONE_DROP) {
            val here = world.state.getBlockState(x, floorY, z) ?: return
            if (BlockSupport.holdsWater(here)) return
            if (here != Blocks.AIR.stateId) {
                if (BlockShapes.isFaceSturdy(here, org.basaltmc.server.registry.BlockFace.UP)) {
                    placeTip(world, x, floorY + 1, z, "up", changes)
                }
                return
            }
            floorY--
        }
    }

    private fun isDripstone(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path == "pointed_dripstone"

    private fun placeTip(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        direction: String,
        changes: MutableList<BlockChange>,
    ) {
        val tip =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("pointed_dripstone"),
                mapOf("vertical_direction" to direction, "thickness" to "tip", "waterlogged" to "false"),
            ) ?: return
        set(world, x, y, z, tip, changes)
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

    /**
     * parity: `NetherPortalBlock.randomTick`, which is where the zombified piglins in an overworld
     * portal come from. The attribute defaults to off and the overworld's dimension type turns it
     * on, so it follows the dimension here.
     *
     * The piglin it spawns stands still: nothing has a brain yet (§6.4). What this owns is the
     * block's own rule, and that is what runs.
     */
    public fun netherPortal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        allocateEntityId: () -> Int,
    ): ServerEntity? {
        if (!world.declaration.dimension.spawnsPortalPiglins) return null
        if (!world.gameRules[GameRuleRegistry.SPAWN_MONSTERS]) return null
        val difficulty = difficultyOf(world)
        if (difficulty <= 0) return null
        if (random.nextInt(PORTAL_ODDS) >= difficulty) return null

        var floorY = y
        while (BlockStateDefinitions.blockOf(world.state.getBlockState(x, floorY, z) ?: 0)?.path == "nether_portal") {
            floorY--
        }
        val ground = world.state.getBlockState(x, floorY, z) ?: return null
        if (!BlockShapes.isFaceSturdy(ground, org.basaltmc.server.registry.BlockFace.UP)) return null
        val type = EntityTypes[Identifier.vanilla("zombified_piglin")] ?: return null
        val entity =
            ServerEntity(
                entityId = allocateEntityId(),
                uuid = Uuid.random(),
                type = Identifier.vanilla("zombified_piglin"),
                protocolType = type.protocolId,
                x = x + 0.5,
                y = (floorY + 1).toDouble(),
                z = z + 0.5,
            )
        return world.state.spawnEntity(entity)
    }

    private fun difficultyOf(world: LoadedWorld): Int =
        when (world.settings.difficulty) {
            "peaceful" -> 0
            "easy" -> 1
            "normal" -> 2
            else -> 3
        }

    private fun shrink(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        leaves: String,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val smaller = BlockStateDefinitions.stateIdOf(block, mapOf("leaves" to leaves), stateId) ?: return
        set(world, x, y, z, smaller, changes)
    }

    private fun isBamboo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path == "bamboo"

    /** Whether a mushroom may sit here: empty, and something it can survive on. */
    private fun fits(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean {
        if (world.state.getBlockState(x, y, z) != Blocks.AIR.stateId) return false
        return NeighbourUpdates.isSupported(world, x, y, z, stateId)
    }

    private fun grow(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        maxAge: Int,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= maxAge) return
        val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
        set(world, x, y, z, older, changes)
    }

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

    private fun offsetX(facing: String): Int =
        if (facing == "west") {
            -1
        } else if (facing == "east") {
            1
        } else {
            0
        }

    private fun offsetZ(facing: String): Int =
        if (facing == "north") {
            -1
        } else if (facing == "south") {
            1
        } else {
            0
        }

    /** Which stem grows what, and what it turns into once it has. */
    private val FRUIT_OF = mapOf("pumpkin_stem" to "pumpkin", "melon_stem" to "melon")
    private val ATTACHED_OF =
        mapOf("pumpkin_stem" to "attached_pumpkin_stem", "melon_stem" to "attached_melon_stem")

    /** parity: `StemBlock.fruitSupportBlocks`, `#minecraft:dirt` plus farmland. */
    private val FRUIT_GROUND =
        setOf(
            "dirt",
            "grass_block",
            "podzol",
            "coarse_dirt",
            "mycelium",
            "rooted_dirt",
            "moss_block",
            "mud",
            "farmland",
        )

    private val HORIZONTAL_NAMES = listOf("north", "south", "west", "east")
    private val FACE_NAMES = listOf("down", "up", "north", "south", "west", "east")

    /** What each climbing plant leaves behind it. parity: `GrowingPlantHeadBlock.getBodyBlock`. */
    private val BODY_OF =
        mapOf(
            "kelp" to "kelp_plant",
            "cave_vines" to "cave_vines_plant",
            "twisting_vines" to "twisting_vines_plant",
            "weeping_vines" to "weeping_vines_plant",
        )

    private const val FULL_DAMPENING = 15
    private const val GROWTH_LIGHT = 9
    private const val COCOA_ODDS = 5
    private const val COCOA_MAX_AGE = 2
    private const val BERRY_ODDS = 5
    private const val BERRY_MAX_AGE = 3
    private const val MUSHROOM_ODDS = 25
    private const val MUSHROOM_CROWD = 5
    private const val STEM_MAX_AGE = 7
    private const val BAMBOO_ODDS = 3
    private const val BAMBOO_MAX_HEIGHT = 16
    private const val BAMBOO_TALL = 11
    private const val BAMBOO_STAGE_CHANCE = 0.25f
    private const val PLANT_MAX_AGE = 25
    private const val AMETHYST_ODDS = 5
    private const val TURTLE_HATCH = 2
    private const val CHORUS_DEAD_AGE = 5
    private const val CHORUS_BRANCH_AGE = 4
    private const val VINE_ODDS = 4
    private const val VINE_CROWD = 5

    /** parity: `SpeleothemBlock.randomTick`'s own chance, its reach and its drop. */
    private const val DRIPSTONE_CHANCE = 0.011377778f
    private const val DRIPSTONE_REACH = 7
    private const val DRIPSTONE_DROP = 10

    /** parity: the `day` timeline's keyframes for `gameplay/eyeblossom_open`. */
    private const val EYEBLOSSOM_OPENS = 12600L
    private const val EYEBLOSSOM_CLOSES = 23401L
    private const val DAY_TICKS = 24000L

    /** parity: `NetherPortalBlock.randomTick`'s `nextInt(2000) < difficulty`. */
    private const val PORTAL_ODDS = 2000

    /** parity: `DriedGhastBlock.randomTick`'s own delay. */
    private const val DRIED_GHAST_DELAY = 5000

    /** parity: `#minecraft:supports_chorus_flower`, which is end stone and little else. */
    private val CHORUS_GROUND = setOf("end_stone")

    /** parity: `EnvironmentAttributes.TURTLE_EGG_HATCH_CHANCE`'s default. */
    private const val TURTLE_HATCH_CHANCE = 0.002f

    /** parity: `ChangeOverTimeBlock`'s own "each block once per day" figure. */
    private const val WEATHER_DAILY_CHANCE = 0.05688889f
    private const val WEATHER_RANGE = 4
}
