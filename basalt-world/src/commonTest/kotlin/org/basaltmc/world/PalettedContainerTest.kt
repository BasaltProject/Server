package org.basaltmc.world

import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class PalettedContainerTest {
    private val size = 4096

    @Test
    fun `starts uniform and costs no data array`() {
        val container = PalettedContainer(size)

        assertEquals(1, container.distinctValueCount)
        assertEquals(0, container.bitsPerValue, "an all-identical container must not allocate entry storage")
        for (i in 0 until size) assertEquals(0, container[i])
    }

    @Test
    fun `stores and reads back a single value`() {
        val container = PalettedContainer(size)
        container[100] = 7

        assertEquals(7, container[100])
        assertEquals(0, container[99])
        assertEquals(0, container[101])
    }

    @Test
    fun `reports whether a write changed anything`() {
        val container = PalettedContainer(size)

        assertTrue(container.set(5, 3))
        assertFalse(container.set(5, 3), "rewriting the same value is not a change")
        assertFalse(container.set(6, 0), "writing the value already implied by a uniform container is not a change")
    }

    @Test
    fun `widens as distinct values are added and keeps every earlier one`() {
        val container = PalettedContainer(size)
        for (i in 0 until 100) container[i] = i

        assertEquals(100, container.distinctValueCount)
        assertTrue(container.bitsPerValue >= 7)
        for (i in 0 until 100) assertEquals(i, container[i], "value at $i survived the widening")
    }

    @Test
    fun `handles entries that straddle two words`() {
        // 5 bits per entry never divides 64 evenly, so entries cross word boundaries constantly.
        val container = PalettedContainer(size)
        for (i in 0 until 20) container[i] = i + 1
        for (i in 0 until size) container[i] = (i % 20) + 1

        for (i in 0 until size) assertEquals((i % 20) + 1, container[i], "mismatch at $i")
    }

    @Test
    fun `matches a plain array over a long random sequence`() {
        val random = Random(20260808)
        val container = PalettedContainer(size)
        val reference = IntArray(size)

        repeat(50_000) {
            val index = random.nextInt(size)
            val value = random.nextInt(0, 300)
            container[index] = value
            reference[index] = value
        }

        for (i in 0 until size) assertEquals(reference[i], container[i], "mismatch at $i")
        assertEquals(reference.toList(), container.toIntArray().toList())
    }

    @Test
    fun `setAll rebuilds the contents and the palette`() {
        val container = PalettedContainer(size)
        for (i in 0 until size) container[i] = i % 500

        val replacement = IntArray(size) { 42 }
        container.setAll(replacement)

        assertEquals(1, container.distinctValueCount, "a uniform replacement must collapse the palette again")
        assertEquals(replacement.toList(), container.toIntArray().toList())
    }

    @Test
    fun `honours a non-zero initial value`() {
        val container = PalettedContainer(64, initialValue = 9)

        assertEquals(9, container[0])
        assertEquals(9, container[63])
        assertFalse(container.set(0, 9))
    }
}
