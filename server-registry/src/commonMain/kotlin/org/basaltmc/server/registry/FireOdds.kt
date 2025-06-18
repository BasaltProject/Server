package org.basaltmc.server.registry

/**
 * How readily each block catches fire and how readily it burns away.
 *
 * parity: the two `Object2IntMap`s `FireBlock` fills at bootstrap. A waterlogged block answers zero
 * to both, whatever it is made of.
 */
public object FireOdds {
    private val ignite: IntArray
    private val burn: IntArray

    init {
        val highest = ALL_FIRE_ODDS.maxOfOrNull { it.first } ?: 0
        ignite = IntArray(highest + 1)
        burn = IntArray(highest + 1)
        for ((block, igniteOdds, burnOdds) in ALL_FIRE_ODDS) {
            ignite[block] = igniteOdds
            burn[block] = burnOdds
        }
    }

    /** How likely this block is to catch when there is fire nearby. */
    public fun igniteOdds(stateId: Int): Int = oddsOf(ignite, stateId)

    /** How likely the fire on it is to consume it. */
    public fun burnOdds(stateId: Int): Int = oddsOf(burn, stateId)

    private fun oddsOf(
        table: IntArray,
        stateId: Int,
    ): Int {
        if (BlockSupport.holdsWater(stateId)) return 0
        val block = BlockSupport.blockRegistryIdOf(stateId)
        return if (block in table.indices) table[block] else 0
    }
}
