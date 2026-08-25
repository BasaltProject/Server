package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom

/** parity: `CocoaBlock.randomTick`, one roll in five, three ages. */
public object CocoaGrowth {
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

    private const val COCOA_ODDS = 5
    private const val COCOA_MAX_AGE = 2
}
