package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Weathering
import org.basaltmc.wire.common.Identifier

/** parity: `ChangeOverTimeBlock.changeOverTime`, copper and the like weathering over time. */
public object WeatheringGrowth {
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

    /** parity: `ChangeOverTimeBlock`'s own "each block once per day" figure. */
    private const val WEATHER_DAILY_CHANCE = 0.05688889f
    private const val WEATHER_RANGE = 4
}
