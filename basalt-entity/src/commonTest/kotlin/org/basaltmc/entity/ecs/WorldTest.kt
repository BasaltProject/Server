package org.basaltmc.entity.ecs

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

class WorldTest {
    @Test
    fun createdEntitiesAreAlive() {
        val world = World()
        val id = world.create()
        assertTrue(world.isAlive(id))
    }

    @Test
    fun destroyedEntitiesAreNotAlive() {
        val world = World()
        val id = world.create()
        world.destroy(id)
        assertFalse(world.isAlive(id))
    }

    @Test
    fun recycledIndexGetsANewGenerationSoTheOldHandleStaysDead() {
        val world = World()
        val first = world.create()
        world.destroy(first)
        val second = world.create()

        assertEquals(first.index, second.index)
        assertNotEquals(first.generation, second.generation)
        assertFalse(world.isAlive(first))
        assertTrue(world.isAlive(second))
    }

    @Test
    fun destroyingAnAlreadyDeadEntityIsANoOp() {
        val world = World()
        val id = world.create()
        world.destroy(id)
        world.destroy(id)
        assertFalse(world.isAlive(id))
    }

    @Test
    fun manyEntitiesGetDistinctIds() {
        val world = World()
        val ids = List(1000) { world.create() }
        assertEquals(ids.toSet().size, ids.size)
    }
}
