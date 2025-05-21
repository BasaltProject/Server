package org.basaltmc.entity.ecs

import org.basaltmc.entity.component.Position
import org.basaltmc.entity.component.Velocity
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.Duration
import kotlin.time.Duration.Companion.milliseconds
import kotlin.time.TimeSource

/**
 * The prototype-and-measure step this project's own roadmap calls for before committing to ECS
 * itself: 10,000 entities, each with [Position] and [Velocity], run through [applyVelocity] for
 * enough ticks to get a stable per-tick timing, checked against the 20 TPS budget (50ms/tick) a
 * real tick loop needs. This only measures the storage/iteration overhead this module owns, not a
 * full server tick (chunk sending, pathfinding, redstone... aren't built yet); the number that
 * matters here is how much of that 50ms budget the ECS layer itself would spend, not the whole
 * budget.
 */
class EcsBenchmarkTest {
    private val entityCount = 10_000
    private val warmupTicks = 20
    private val measuredTicks = 200
    private val tickBudget = 50.milliseconds

    @Test
    fun tenThousandEntitiesStayWellUnderTheTickBudget() {
        val world = World()
        val positions = ComponentStore<Position>()
        val velocities = ComponentStore<Velocity>()

        for (i in 0 until entityCount) {
            val entity = world.create()
            positions.set(entity.index, Position(0.0, 64.0, 0.0))
            velocities.set(entity.index, Velocity(0.1, 0.0, 0.05))
        }

        repeat(warmupTicks) { applyVelocity(positions, velocities) }

        val timeSource = TimeSource.Monotonic
        var total = Duration.ZERO
        var worst = Duration.ZERO
        repeat(measuredTicks) {
            val start = timeSource.markNow()
            applyVelocity(positions, velocities)
            val elapsed = start.elapsedNow()
            total += elapsed
            if (elapsed > worst) worst = elapsed
        }
        val average = total / measuredTicks

        println(
            "ECS prototype benchmark: $entityCount entities, $measuredTicks ticks " +
                "(after $warmupTicks warmup) -> average ${average.inWholeMicroseconds}us/tick, " +
                "worst ${worst.inWholeMicroseconds}us/tick (20 TPS budget: ${tickBudget.inWholeMilliseconds}ms/tick)",
        )

        assertTrue(average < tickBudget, "average tick time $average exceeded the 20 TPS budget of $tickBudget")

        val expectedTicks = warmupTicks + measuredTicks
        val entity0Position = positions.get(0)!!
        assertApproxEquals(0.1 * expectedTicks, entity0Position.x)
        assertApproxEquals(64.0, entity0Position.y)
        assertApproxEquals(0.05 * expectedTicks, entity0Position.z)
    }

    private fun assertApproxEquals(
        expected: Double,
        actual: Double,
        tolerance: Double = 1e-6,
    ) {
        assertTrue(
            kotlin.math.abs(expected - actual) <= tolerance,
            "expected $expected but was $actual (tolerance $tolerance)",
        )
    }
}
