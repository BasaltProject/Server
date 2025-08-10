package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** parity: `ChorusFlowerBlock.randomTick`. */
public object ChorusFlowerGrowth {
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

    private const val CHORUS_DEAD_AGE = 5
    private const val CHORUS_BRANCH_AGE = 4

    /** parity: `#minecraft:supports_chorus_flower`, which is end stone and little else. */
    private val CHORUS_GROUND = setOf("end_stone")
}
