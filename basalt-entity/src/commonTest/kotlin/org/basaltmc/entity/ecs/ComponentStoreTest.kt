package org.basaltmc.entity.ecs

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ComponentStoreTest {
    @Test
    fun setThenGetReturnsTheValue() {
        val store = ComponentStore<String>()
        store.set(5, "hello")
        assertEquals("hello", store.get(5))
        assertTrue(store.has(5))
    }

    @Test
    fun getOnAnAbsentEntityReturnsNull() {
        val store = ComponentStore<String>()
        assertNull(store.get(42))
        assertFalse(store.has(42))
    }

    @Test
    fun settingTwiceOverwritesRatherThanDuplicating() {
        val store = ComponentStore<Int>()
        store.set(3, 1)
        store.set(3, 2)
        assertEquals(2, store.get(3))
        assertEquals(1, store.size)
    }

    @Test
    fun removeSwapsWithTheLastEntryAndShrinksDense() {
        val store = ComponentStore<String>()
        store.set(0, "a")
        store.set(1, "b")
        store.set(2, "c")

        store.remove(1)

        assertEquals(2, store.size)
        assertFalse(store.has(1))
        assertEquals("a", store.get(0))
        assertEquals("c", store.get(2))
    }

    @Test
    fun removingAnAbsentEntityIsANoOp() {
        val store = ComponentStore<String>()
        store.set(0, "a")
        store.remove(99)
        assertEquals(1, store.size)
    }

    @Test
    fun forEachVisitsEveryEntryExactlyOnce() {
        val store = ComponentStore<Int>()
        for (i in 0 until 100) store.set(i, i * 10)

        val visited = mutableMapOf<Int, Int>()
        store.forEach { entityIndex, value -> visited[entityIndex] = value }

        assertEquals(100, visited.size)
        for (i in 0 until 100) assertEquals(i * 10, visited[i])
    }

    @Test
    fun reAddingAfterRemoveWorks() {
        val store = ComponentStore<String>()
        store.set(0, "a")
        store.remove(0)
        assertFalse(store.has(0))
        store.set(0, "b")
        assertEquals("b", store.get(0))
        assertEquals(1, store.size)
    }

    @Test
    fun growsPastInitialCapacityCorrectly() {
        val store = ComponentStore<Int>()
        for (i in 0 until 10_000) store.set(i, i)
        assertEquals(10_000, store.size)
        for (i in 0 until 10_000) assertEquals(i, store.get(i))
    }
}
