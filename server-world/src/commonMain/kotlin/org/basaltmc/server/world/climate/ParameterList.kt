package org.basaltmc.server.world.climate

import org.basaltmc.server.world.GenerationMetrics

/**
 * // parity: net.minecraft.world.level.biome.Climate.ParameterList. Finds the value whose
 * [ParameterPoint] is closest (lowest [ParameterPoint.fitness]) to a sampled [TargetPoint]:
 * vanilla's actual biome-selection algorithm, independent of which biomes or noise router feed it.
 *
 * [findValue] goes through the same [RTree] index vanilla builds. [findValueBruteForce] keeps
 * vanilla's own linear scan (`findValueBruteForce`) around as the oracle the index is tested
 * against: the two must agree on every target, and the real Overworld list is large enough
 * (thousands of regions) that scanning it per lookup dominated chunk generation outright.
 */
public class ParameterList<T>(
    public val values: List<Pair<ParameterPoint, T>>,
) {
    init {
        require(values.isNotEmpty()) { "Need at least one value to build the search tree." }
    }

    private val index = RTree.create(values)

    public fun findValue(target: TargetPoint): T {
        if (GenerationMetrics.enabled) GenerationMetrics.countBiomeLookup()
        return index.search(target.toParameterArray())
    }

    /** // parity: Climate.ParameterList.findValueBruteForce. The reference the index is checked against. */
    public fun findValueBruteForce(target: TargetPoint): T {
        val iterator = values.iterator()
        var best = iterator.next()
        var bestFitness = best.first.fitness(target)
        for (candidate in iterator) {
            val fitness = candidate.first.fitness(target)
            if (fitness < bestFitness) {
                bestFitness = fitness
                best = candidate
            }
        }
        return best.second
    }
}
