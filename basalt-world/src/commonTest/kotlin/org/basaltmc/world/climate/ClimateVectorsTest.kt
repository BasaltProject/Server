package org.basaltmc.world.climate

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `Climate` bytecode, not derived from this project's own code. */
class ClimateVectorsTest {
    @Test
    fun parameterDistanceMatchesVanilla() {
        val p1 = Parameter.span(-1.0f, 1.0f)
        val p2 = Parameter.point(0.5f)
        assertEquals(-10000L, p1.min)
        assertEquals(10000L, p1.max)
        assertEquals(10000L, p1.distance(20000L))
        assertEquals(10000L, p1.distance(-20000L))
        assertEquals(0L, p1.distance(0L))
        assertEquals(0L, p1.distance(p2))
    }

    @Test
    fun parameterPointFitnessMatchesVanilla() {
        val point = parameters(0.1f, 0.2f, 0.3f, -0.1f, 0.0f, 0.5f, 0.0f)
        val targetPoint = target(0.15f, 0.25f, 0.1f, -0.05f, 0.02f, 0.4f)
        assertEquals(5790000L, point.fitness(targetPoint))
    }

    @Test
    fun parameterListFindsTheNearestVanillaMatch() {
        val values =
            listOf(
                parameters(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 0.0f) to 0,
                parameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) to 1,
                parameters(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f) to 2,
            )
        val list = ParameterList(values)
        val near1 = target(0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f)
        val near0 = target(-0.8f, -0.9f, -0.85f, -0.95f, -0.7f, -0.6f)
        val nearMid = target(0.05f, -0.05f, 0.02f, 0.0f, -0.02f, 0.01f)
        assertEquals(2, list.findValue(near1))
        assertEquals(0, list.findValue(near0))
        assertEquals(1, list.findValue(nearMid))
    }
}
