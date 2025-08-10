package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks

/** parity: `MushroomBlock.randomTick`. */
public object MushroomGrowth {
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

    private const val MUSHROOM_ODDS = 25
    private const val MUSHROOM_CROWD = 5
}
