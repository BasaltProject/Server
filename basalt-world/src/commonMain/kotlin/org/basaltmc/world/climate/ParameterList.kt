package org.basaltmc.world.climate

/**
 * // parity: net.minecraft.world.level.biome.Climate.ParameterList. Finds the value whose
 * [ParameterPoint] is closest (lowest [ParameterPoint.fitness]) to a sampled [TargetPoint]:
 * vanilla's actual biome-selection algorithm, independent of which biomes or noise router feed
 * it. Only vanilla's brute-force search (`findValueBruteForce`, a linear scan) is implemented:
 * the real `Climate.RTree` spatial index vanilla builds over the same values is a performance
 * optimization only (§5.4 of this project's own roadmap excludes performance from the parity
 * contract), not a behavior difference, so it is out of scope until it is actually needed.
 */
public class ParameterList<T>(
    public val values: List<Pair<ParameterPoint, T>>,
) {
    init {
        require(values.isNotEmpty()) { "Need at least one value to build the search tree." }
    }

    public fun findValue(target: TargetPoint): T {
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
