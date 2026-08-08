package org.basaltmc.world.climate

import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Biome
import kotlin.test.Test
import kotlin.test.assertEquals

class RTreeTest {
    private val overworld = OverworldBiomeBuilder().build()

    private fun randomTarget(random: XoroshiroRandom): TargetPoint =
        target(
            temperature = random.nextDouble().toFloat() * 4f - 2f,
            humidity = random.nextDouble().toFloat() * 4f - 2f,
            continentalness = random.nextDouble().toFloat() * 4f - 2f,
            erosion = random.nextDouble().toFloat() * 4f - 2f,
            depth = random.nextDouble().toFloat() * 4f - 2f,
            weirdness = random.nextDouble().toFloat() * 4f - 2f,
        )

    private fun bestFitness(point: TargetPoint): Long = overworld.values.minOf { it.first.fitness(point) }

    private fun fitnessOf(
        biome: Biome,
        point: TargetPoint,
    ): Long = overworld.values.filter { it.second == biome }.minOf { it.first.fitness(point) }

    /**
     * The property that actually has to hold: the index never returns a region that is farther than
     * the closest one. A mis-sorted bucket or a bad bounding box would prune away the real winner
     * and show up here immediately, run against the real Overworld list.
     *
     * Not asserted as equality with [ParameterList.findValueBruteForce], because the two genuinely
     * differ when several distinct biomes sit at the exact same distance: the scan takes the first
     * in list order, the index takes the first it reaches. Vanilla has the same ambiguity and
     * resolves it from a `ThreadLocal` holding the previous lookup's result, so it does not have a
     * single reproducible answer to match either. See [RTree]'s own KDoc.
     */
    @Test
    fun `index always finds a closest region on the real overworld list`() {
        val random = XoroshiroRandom(20260808L)
        repeat(5000) {
            val point = randomTarget(random)
            assertEquals(
                bestFitness(point),
                fitnessOf(overworld.findValue(point), point),
                "index returned a region that was not closest, for $point",
            )
        }
    }

    @Test
    fun `index only ever differs from the brute force scan on exact ties`() {
        val random = XoroshiroRandom(4903L)
        repeat(5000) {
            val point = randomTarget(random)
            val brute = overworld.findValueBruteForce(point)
            val indexed = overworld.findValue(point)
            if (brute != indexed) {
                assertEquals(
                    fitnessOf(brute, point),
                    fitnessOf(indexed, point),
                    "index and scan disagreed by more than a tie, for $point",
                )
            }
        }
    }

    /** Targets right at the extremes, where bounding boxes stop overlapping and pruning is most aggressive. */
    @Test
    fun `index finds a closest region at the edges of the parameter space`() {
        for (extreme in listOf(-2f, -1f, 0f, 1f, 2f)) {
            val point = target(extreme, extreme, extreme, extreme, extreme, extreme)
            assertEquals(bestFitness(point), fitnessOf(overworld.findValue(point), point))
        }
    }

    @Test
    fun `a single value list always resolves to that value`() {
        val only = parameters(0f, 0f, 0f, 0f, 0f, 0f, 0f)
        val list = ParameterList(listOf(only to "only"))
        assertEquals("only", list.findValue(target(1f, -1f, 0.5f, 0f, 0f, 0f)))
    }

    @Test
    fun `an exact hit resolves to its own region`() {
        val here = parameters(0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0f)
        val far = parameters(-1f, -1f, -1f, -1f, -1f, -1f, 0f)
        val list = ParameterList(listOf(far to "far", here to "here"))
        assertEquals("here", list.findValue(target(0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f)))
    }
}
