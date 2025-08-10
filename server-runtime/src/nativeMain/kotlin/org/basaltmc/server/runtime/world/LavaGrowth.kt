package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.FireOdds
import org.basaltmc.wire.common.Identifier

/** parity: `LavaFluid.randomTick`, lava setting fire to what it can reach. */
public object LavaGrowth {
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
}
