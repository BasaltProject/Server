package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

class DensityCacheTest {
    /** Reports the position it was asked for, so a test can see exactly where the cache sampled. */
    private class PositionEcho : DensityFunction {
        var calls: Int = 0

        override fun compute(context: FunctionContext): Double {
            calls++
            return context.blockX * 1_000_000.0 + context.blockY * 1_000.0 + context.blockZ
        }

        override fun minValue(): Double = Double.NEGATIVE_INFINITY

        override fun maxValue(): Double = Double.POSITIVE_INFINITY
    }

    @Test
    fun `flat cache quantizes to the quart grid and samples at y zero`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        // parity: NoiseChunk.FlatCache fills at QuartPos.toBlock(quart) with y = 0, so a query at
        // (5, 70, 6) resolves to the value sampled at (4, 0, 4).
        val value = cache.flatCache(0, FunctionContext(5, 70, 6, cache), echo)

        assertEquals(4 * 1_000_000.0 + 0 * 1_000.0 + 4, value)
        assertEquals(1, echo.calls)
    }

    @Test
    fun `flat cache serves every position inside the same quart from one sample`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        val first = cache.flatCache(0, FunctionContext(4, 0, 4, cache), echo)
        for (x in 4..7) {
            for (z in 4..7) {
                for (y in intArrayOf(-64, 0, 100, 319)) {
                    assertEquals(first, cache.flatCache(0, FunctionContext(x, y, z, cache), echo))
                }
            }
        }
        assertEquals(1, echo.calls)
    }

    @Test
    fun `flat cache keeps neighbouring quarts apart`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        val here = cache.flatCache(0, FunctionContext(0, 0, 0, cache), echo)
        val next = cache.flatCache(0, FunctionContext(4, 0, 0, cache), echo)

        assertEquals(0.0, here)
        assertEquals(4 * 1_000_000.0, next)
        assertEquals(2, echo.calls)
    }

    /** // parity: NoiseChunk.FlatCache.compute's own bounds check, which falls back to the real context. */
    @Test
    fun `flat cache falls through outside its own chunk`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        val outside = cache.flatCache(0, FunctionContext(-32, 70, 0, cache), echo)

        assertEquals(-32 * 1_000_000.0 + 70 * 1_000.0, outside)
    }

    @Test
    fun `flat cache falls through for a marker with no slot of its own`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        val value = cache.flatCache(7, FunctionContext(5, 70, 6, cache), echo)

        assertEquals(5 * 1_000_000.0 + 70 * 1_000.0 + 6, value)
    }

    @Test
    fun `flat cache keeps slots independent`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 2)
        val first = PositionEcho()
        val second = PositionEcho()

        cache.flatCache(0, FunctionContext(0, 0, 0, cache), first)
        cache.flatCache(1, FunctionContext(0, 0, 0, cache), second)

        assertEquals(1, first.calls)
        assertEquals(1, second.calls)
    }

    @Test
    fun `cache2d memoizes the exact last position without quantizing`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        val first = cache.cache2d(0, FunctionContext(5, 70, 6, cache), echo)
        val repeat = cache.cache2d(0, FunctionContext(5, 999, 6, cache), echo)

        assertEquals(5 * 1_000_000.0 + 70 * 1_000.0 + 6, first)
        assertEquals(first, repeat)
        assertEquals(1, echo.calls)
    }

    @Test
    fun `cache2d recomputes once the horizontal position moves`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = 1)
        val echo = PositionEcho()

        cache.cache2d(0, FunctionContext(5, 0, 6, cache), echo)
        cache.cache2d(0, FunctionContext(6, 0, 6, cache), echo)
        cache.cache2d(0, FunctionContext(5, 0, 6, cache), echo)

        assertEquals(3, echo.calls)
    }

    @Test
    fun `cache2d handles negative coordinates without collisions`() {
        val cache = DensityCache(chunkX = -1, chunkZ = -1, slotCount = 1)
        val echo = PositionEcho()

        val negative = cache.cache2d(0, FunctionContext(-1, 0, -1, cache), echo)
        val positive = cache.cache2d(0, FunctionContext(-1, 0, 1, cache), echo)

        assertEquals(-1 * 1_000_000.0 - 1, negative)
        assertEquals(-1 * 1_000_000.0 + 1, positive)
        assertEquals(2, echo.calls)
    }

    @Test
    fun `a marker with no cache in context evaluates straight through`() {
        val echo = PositionEcho()
        val marker = Marker(MarkerType.FLAT_CACHE, echo)

        val value = marker.compute(FunctionContext(5, 70, 6))

        assertEquals(5 * 1_000_000.0 + 70 * 1_000.0 + 6, value)
        assertEquals(1, echo.calls)
    }

    @Test
    fun `markers this project does not model yet stay pass-through`() {
        val cache = DensityCache(chunkX = 0, chunkZ = 0, slotCount = Marker.slotCount + 4)
        val echo = PositionEcho()
        val marker = Marker(MarkerType.INTERPOLATED, echo)

        marker.compute(FunctionContext(5, 70, 6, cache))
        marker.compute(FunctionContext(5, 70, 6, cache))

        assertEquals(2, echo.calls)
    }
}
