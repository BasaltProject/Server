package org.basaltmc.server.entity.ecs

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals

class EntityIdTest {
    @Test
    fun indexAndGenerationRoundTrip() {
        val id = EntityId.of(index = 12345, generation = 7)
        assertEquals(12345, id.index)
        assertEquals(7, id.generation)
    }

    @Test
    fun sameIndexDifferentGenerationAreNotEqual() {
        val a = EntityId.of(index = 1, generation = 0)
        val b = EntityId.of(index = 1, generation = 1)
        assertNotEquals(a, b)
    }

    @Test
    fun zeroIndexAndGenerationRoundTrip() {
        val id = EntityId.of(index = 0, generation = 0)
        assertEquals(0, id.index)
        assertEquals(0, id.generation)
    }
}
