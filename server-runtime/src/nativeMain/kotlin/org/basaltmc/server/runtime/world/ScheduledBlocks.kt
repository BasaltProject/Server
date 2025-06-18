package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * What blocks do on a turn they asked for, rather than one the random tick handed them.
 *
 * parity: the `tick` half of the block classes. Most of them are "if it cannot survive, break it",
 * which the neighbour update already does the moment the support goes, a tick earlier than vanilla
 * and to the same end. What is here is the rest: the ones that change into something else, count
 * their way out from a support, or melt.
 */
public object ScheduledBlocks {
    /** What a scheduled tick on each of these is filed under. */
    public val FROSTED_ICE: Identifier = Identifier.vanilla("frosted_ice")
    public val DRIED_GHAST: Identifier = Identifier.vanilla("dried_ghast")
    public val BUBBLE_COLUMN: Identifier = Identifier.vanilla("bubble_column")

    /**
     * parity: `FarmlandBlock.turnToDirt` and `DirtPathBlock.tick`, both of which give way to plain
     * dirt when something solid lands on top rather than breaking.
     */
    public fun coveredSoil(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        if (path != "farmland" && path != "dirt_path") return null
        val above = world.state.getBlockState(x, y + 1, z) ?: return null
        if (!BlockShapes.isSolidState(above)) return null
        // parity: `shouldMaintainFarmland`, the crop that holds tilled soil together.
        if (path == "farmland" &&
            org.basaltmc.server.registry.BlockConnections
                .maintainsFarmland(above)
        ) {
            return null
        }
        if (!world.state.setBlockState(x, y, z, Blocks.DIRT.stateId)) return null
        return Blocks.DIRT.stateId
    }

    /**
     * parity: `CoralBlock.tick` and the three `BaseCoralPlantTypeBlock` ones: coral out of water
     * turns into its dead twin instead of breaking.
     */
    public fun coral(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        if (!path.endsWith("coral") &&
            !path.endsWith("coral_block") &&
            !path.endsWith("coral_fan") &&
            !path.endsWith("coral_wall_fan")
        ) {
            return null
        }
        if (path.startsWith("dead_")) return null
        if (waterAbout(world, x, y, z, stateId)) return null
        val dead = "dead_$path"
        val properties = BlockStateDefinitions.propertiesOf(stateId).filterKeys { it != "waterlogged" }
        val deadState =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla(dead), properties)
                ?: Blocks.REGISTRY.idOf(Identifier.vanilla(dead))
                ?: return null
        if (!world.state.setBlockState(x, y, z, deadState)) return null
        return deadState
    }

    /** parity: `BaseCoralPlantTypeBlock.scanForWater`, its own state included. */
    private fun waterAbout(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean {
        if (BlockSupport.holdsWater(stateId)) return true
        for (face in BlockFace.entries) {
            val neighbour =
                world.state.getBlockState(
                    x + offsetX(face),
                    y + offsetY(face),
                    z + offsetZ(face),
                ) ?: continue
            if (BlockSupport.holdsWater(neighbour)) return true
        }
        return false
    }

    /**
     * parity: `ScaffoldingBlock.getDistance` and its `tick`: it counts out from whatever holds it,
     * and at seven it comes down.
     */
    public fun scaffolding(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId)?.path != "scaffolding") return null
        val distance = scaffoldDistance(world, x, y, z)
        val bottom = distance > 0 && !isScaffolding(world, x, y - 1, z)
        val reshaped =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("scaffolding"),
                mapOf("distance" to distance.toString(), "bottom" to bottom.toString()),
                stateId,
            ) ?: return null
        if (reshaped == stateId) return null
        if (!world.state.setBlockState(x, y, z, reshaped)) return null
        return reshaped
    }

    /** Whether scaffolding here is holding on to anything at all. */
    public fun scaffoldingHasFallen(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = scaffoldDistance(world, x, y, z) >= SCAFFOLD_REACH

    private fun scaffoldDistance(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val below = world.state.getBlockState(x, y - 1, z) ?: return SCAFFOLD_REACH
        var distance = SCAFFOLD_REACH
        if (isScaffolding(world, x, y - 1, z)) {
            distance = BlockStateDefinitions.propertiesOf(below)["distance"]?.toIntOrNull() ?: SCAFFOLD_REACH
        } else if (BlockShapes.isFaceSturdy(below, BlockFace.UP)) {
            return 0
        }
        for (face in listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)) {
            val sideX = x + offsetX(face)
            val sideZ = z + offsetZ(face)
            if (!isScaffolding(world, sideX, y, sideZ)) continue
            val side = world.state.getBlockState(sideX, y, sideZ) ?: continue
            val theirs = BlockStateDefinitions.propertiesOf(side)["distance"]?.toIntOrNull() ?: continue
            distance = minOf(distance, theirs + 1)
            if (distance == 1) break
        }
        return distance
    }

    private fun isScaffolding(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path == "scaffolding"

    /**
     * parity: `FrostedIceBlock.tick`: it thins a step at a time in the light and takes its
     * neighbours with it, and the ones that will not thin yet are asked again shortly.
     */
    public fun frostedIce(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        if (BlockStateDefinitions.blockOf(stateId)?.path != "frosted_ice") return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (random.nextInt(3) != 0 && neighbouringIce(world, x, y, z) >= FROSTED_NEIGHBOURS) {
            world.state.scheduleBlockTick(x, y, z, FROSTED_ICE, FROSTED_RETRY)
            return
        }
        if (WorldLight.rawBrightness(world, x, y, z) <= MELT_LIGHT - age - BlockShapes.dampening(stateId)) {
            world.state.scheduleBlockTick(x, y, z, FROSTED_ICE, FROSTED_RETRY)
            return
        }
        if (!thin(world, x, y, z, age, changes)) return
        for (face in BlockFace.entries) {
            val nx = x + offsetX(face)
            val ny = y + offsetY(face)
            val nz = z + offsetZ(face)
            if (BlockStateDefinitions.blockOf(world.state.getBlockState(nx, ny, nz) ?: 0)?.path != "frosted_ice") {
                continue
            }
            world.state.scheduleBlockTick(nx, ny, nz, FROSTED_ICE, FROSTED_RETRY + random.nextInt(FROSTED_RETRY))
        }
    }

    /** One step thinner, or gone. parity: `FrostedIceBlock.slightlyMelt`. */
    private fun thin(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        age: Int,
        changes: MutableList<BlockChange>,
    ): Boolean {
        if (age < FROSTED_MAX_AGE) {
            val thinner =
                BlockStateDefinitions.stateIdOf(FROSTED_ICE, mapOf("age" to (age + 1).toString())) ?: return false
            if (!world.state.setBlockState(x, y, z, thinner)) return false
            changes += BlockChange(x, y, z, thinner)
            world.state.scheduleBlockTick(x, y, z, FROSTED_ICE, FROSTED_RETRY)
            return true
        }
        if (!world.state.setBlockState(x, y, z, Blocks.WATER.stateId)) return false
        changes += BlockChange(x, y, z, Blocks.WATER.stateId)
        return true
    }

    private fun neighbouringIce(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        var count = 0
        for (face in BlockFace.entries) {
            val neighbour =
                world.state.getBlockState(x + offsetX(face), y + offsetY(face), z + offsetZ(face)) ?: continue
            if (BlockStateDefinitions.blockOf(neighbour)?.path == "frosted_ice") count++
        }
        return count
    }

    /**
     * parity: `DriedGhastBlock.tick`: soaked it fills up, dry it dries out, and it keeps asking for
     * the next turn while there is anything left to change.
     */
    public fun driedGhast(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        if (block.path != "dried_ghast") return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val hydration = properties["hydration"]?.toIntOrNull() ?: return
        val soaked = properties["waterlogged"] == "true"
        val wanted =
            when {
                soaked && hydration < DRIED_GHAST_MAX -> hydration + 1
                !soaked && hydration > 0 -> hydration - 1
                else -> return
            }
        val changed =
            BlockStateDefinitions.stateIdOf(block, mapOf("hydration" to wanted.toString()), stateId) ?: return
        if (!world.state.setBlockState(x, y, z, changed)) return
        changes += BlockChange(x, y, z, changed)
        world.state.scheduleBlockTick(x, y, z, DRIED_GHAST, DRIED_GHAST_DELAY)
    }

    /**
     * parity: `BubbleColumnBlock.updateColumn`: soul sand pushes a column up through the water
     * above it and magma drags one down, and the column climbs while there is water to fill.
     *
     * What the column then does to whoever swims into it is `entityInside`, and that waits for the
     * entities of §6.4. The blocks themselves are this system's.
     */
    public fun bubbleColumn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val here = world.state.getBlockState(x, y, z) ?: return
        if (!canOccupy(here)) return
        val below = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y - 1, z) ?: 0)?.path
        val column =
            when {
                below == "bubble_column" -> world.state.getBlockState(x, y - 1, z) ?: return
                below == "soul_sand" -> columnState(dragDown = false) ?: return
                below == "magma_block" -> columnState(dragDown = true) ?: return
                // Nothing feeding it any more: what was a column goes back to being water.
                BlockStateDefinitions.blockOf(here)?.path == "bubble_column" -> Blocks.WATER.stateId
                else -> return
            }
        var level = y
        while (canOccupy(world.state.getBlockState(x, level, z) ?: return)) {
            if (world.state.getBlockState(x, level, z) == column) {
                level++
                continue
            }
            if (!world.state.setBlockState(x, level, z, column)) return
            changes += BlockChange(x, level, z, column)
            level++
        }
    }

    /** parity: `BubbleColumnBlock.canOccupy`: water, or a column already. */
    private fun canOccupy(stateId: Int): Boolean {
        if (stateId == Blocks.WATER.stateId) return true
        return BlockStateDefinitions.blockOf(stateId)?.path == "bubble_column"
    }

    private fun columnState(dragDown: Boolean): Int? =
        BlockStateDefinitions.stateIdOf(
            Identifier.vanilla("bubble_column"),
            mapOf("drag" to dragDown.toString()),
        )

    private fun offsetX(face: BlockFace): Int =
        if (face == BlockFace.WEST) {
            -1
        } else if (face == BlockFace.EAST) {
            1
        } else {
            0
        }

    private fun offsetY(face: BlockFace): Int =
        if (face == BlockFace.DOWN) {
            -1
        } else if (face == BlockFace.UP) {
            1
        } else {
            0
        }

    private fun offsetZ(face: BlockFace): Int =
        if (face == BlockFace.NORTH) {
            -1
        } else if (face == BlockFace.SOUTH) {
            1
        } else {
            0
        }

    /** parity: `ScaffoldingBlock`'s own reach, and `FrostedIceBlock`'s numbers. */
    private const val SCAFFOLD_REACH = 7
    private const val FROSTED_MAX_AGE = 3
    private const val FROSTED_NEIGHBOURS = 4
    private const val FROSTED_RETRY = 20
    private const val MELT_LIGHT = 11
    private const val DRIED_GHAST_MAX = 3
    private const val DRIED_GHAST_DELAY = 5000
}
