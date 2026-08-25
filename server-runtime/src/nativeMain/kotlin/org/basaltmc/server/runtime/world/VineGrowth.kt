package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks

/** parity: `VineBlock.randomTick`. */
public object VineGrowth {
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

    private const val VINE_ODDS = 4
    private const val VINE_CROWD = 5
}
