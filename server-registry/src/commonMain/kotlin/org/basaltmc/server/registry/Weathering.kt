package org.basaltmc.server.registry

/**
 * What copper turns into as it weathers, and how far along each block already is.
 *
 * parity: `WeatheringCopper.NEXT_BY_BLOCK`, which is a map of blocks and not a naming rule: a
 * chiseled copper and a copper golem statue weather through their own chains.
 */
public object Weathering {
    private val next: Map<String, String> = ALL_WEATHERING_STEPS.toMap()

    /** How weathered a block already is: 0 for fresh copper, 3 for oxidized. */
    private val stages: Map<String, Int> by lazy {
        val previous = next.entries.associate { (from, to) -> to to from }
        val byBlock = HashMap<String, Int>()
        for (block in next.keys + next.values) {
            var stage = 0
            var current = block
            while (true) {
                val earlier = previous[current] ?: break
                current = earlier
                stage++
            }
            byBlock[block] = stage
        }
        byBlock
    }

    public fun next(block: String): String? = next[block]

    public fun stageOf(block: String): Int? = stages[block]

    public fun weathers(block: String): Boolean = block in stages
}
