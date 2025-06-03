package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.spline.BoundedFloatFunction
import org.basaltmc.server.world.spline.CubicSpline
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Verifies [TerrainProvider] against real golden vectors captured from the actual 26.2 server jar
 * (`TerrainProvider.overworldOffset`/`overworldFactor`/`overworldJaggedness`, `amplified = false`),
 * via a small Java harness compiled directly against the decompiled jar classes: not vectors
 * derived from memory or guessed, the real jar computed these numbers.
 */
class TerrainProviderTest {
    private class Ctx(
        val continentalness: Float,
        val erosion: Float,
        val ridges: Float,
        val weirdness: Float,
    )

    private fun axis(f: (Ctx) -> Float): BoundedFloatFunction<Ctx> =
        object : BoundedFloatFunction<Ctx> {
            override fun apply(c: Ctx): Float = f(c)

            override fun minValue(): Float = -1.0f

            override fun maxValue(): Float = 1.0f
        }

    private val continents = axis { it.continentalness }
    private val erosion = axis { it.erosion }
    private val ridges = axis { it.ridges }
    private val weirdness = axis { it.weirdness }

    private val offsetSpline =
        TerrainProvider.overworldOffset<Ctx, BoundedFloatFunction<Ctx>>(
            continents,
            erosion,
            ridges,
        )
    private val factorSpline =
        TerrainProvider.overworldFactor<Ctx, BoundedFloatFunction<Ctx>>(
            continents,
            erosion,
            weirdness,
            ridges,
        )
    private val jaggednessSpline =
        TerrainProvider.overworldJaggedness<Ctx, BoundedFloatFunction<Ctx>>(
            continents,
            erosion,
            weirdness,
            ridges,
        )

    private fun sample(
        spline: CubicSpline<Ctx, BoundedFloatFunction<Ctx>>,
        c: Float,
        e: Float,
        r: Float,
        w: Float,
    ): Float = spline.sample(Ctx(c, e, r, w))

    @Test
    fun declaredBoundsMatchTheRealJar() {
        assertEquals(-0.325f, offsetSpline.minValue())
        assertEquals(1.5f, offsetSpline.maxValue())
        assertEquals(0.625f, factorSpline.minValue())
        assertEquals(6.3f, factorSpline.maxValue())
        assertEquals(0.0f, jaggednessSpline.minValue())
        assertEquals(0.63f, jaggednessSpline.maxValue())
    }

    @Test
    fun offsetMatchesRealJarSamples() {
        assertEquals(-0.2222f, sample(offsetSpline, -0.9f, -0.3f, 0.2f, 0.1f), 1e-6f)
        assertEquals(0.01f, sample(offsetSpline, 0.1f, 0.2f, -0.1f, 0.0f), 1e-6f)
        assertEquals(0.4582061f, sample(offsetSpline, 0.6f, -0.2f, 0.5f, -0.4f), 1e-6f)
        assertEquals(-0.0191036f, sample(offsetSpline, 0.05f, 0.5f, -0.6f, 0.3f), 1e-6f)
        assertEquals(-0.12f, sample(offsetSpline, -0.2f, -0.75f, 0.9f, -0.9f), 1e-6f)
        assertEquals(0.1f, sample(offsetSpline, 1.0f, 1.0f, 1.0f, 1.0f), 1e-6f)
        assertEquals(-0.2222f, sample(offsetSpline, -1.0f, -1.0f, -1.0f, -1.0f), 1e-6f)
        assertEquals(0.0149045f, sample(offsetSpline, 0.3f, 0.05f, -0.02f, 0.02f), 1e-6f)
    }

    @Test
    fun factorMatchesRealJarSamples() {
        assertEquals(3.95f, sample(factorSpline, -0.9f, -0.3f, 0.2f, 0.1f), 1e-4f)
        assertEquals(5.495f, sample(factorSpline, 0.1f, 0.2f, -0.1f, 0.0f), 1e-3f)
        assertEquals(5.3588891f, sample(factorSpline, 0.6f, -0.2f, 0.5f, -0.4f), 1e-4f)
        assertEquals(3.4217811f, sample(factorSpline, 0.05f, 0.5f, -0.6f, 0.3f), 1e-4f)
        assertEquals(3.95f, sample(factorSpline, -0.2f, -0.75f, 0.9f, -0.9f), 1e-4f)
        assertEquals(4.69f, sample(factorSpline, 1.0f, 1.0f, 1.0f, 1.0f), 1e-4f)
        assertEquals(3.95f, sample(factorSpline, -1.0f, -1.0f, -1.0f, -1.0f), 1e-4f)
        assertEquals(5.3746529f, sample(factorSpline, 0.3f, 0.05f, -0.02f, 0.02f), 1e-4f)
    }

    @Test
    fun jaggednessMatchesRealJarSamples() {
        assertEquals(0.0f, sample(jaggednessSpline, -0.9f, -0.3f, 0.2f, 0.1f))
        assertEquals(0.0f, sample(jaggednessSpline, 0.1f, 0.2f, -0.1f, 0.0f))
        assertEquals(0.0f, sample(jaggednessSpline, 0.6f, -0.2f, 0.5f, -0.4f))
        assertEquals(0.0f, sample(jaggednessSpline, 0.05f, 0.5f, -0.6f, 0.3f))
        assertEquals(0.0f, sample(jaggednessSpline, -0.2f, -0.75f, 0.9f, -0.9f))
        assertEquals(0.0f, sample(jaggednessSpline, 1.0f, 1.0f, 1.0f, 1.0f))
        assertEquals(0.0f, sample(jaggednessSpline, -1.0f, -1.0f, -1.0f, -1.0f))
        assertEquals(0.0f, sample(jaggednessSpline, 0.3f, 0.05f, -0.02f, 0.02f))
    }
}
