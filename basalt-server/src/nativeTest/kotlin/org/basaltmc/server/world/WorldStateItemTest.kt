package org.basaltmc.server.world

import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
class WorldStateItemTest {
    private fun world(): WorldState = WorldState(listOf(Chunk(0, 0)))

    private fun droppedItem(
        entityId: Int = 1,
        item: Identifier = Identifier.vanilla("diamond"),
        count: Int = 1,
        x: Double = 8.5,
        y: Double = 64.0,
        z: Double = 8.5,
    ) = WorldState.DroppedItem(entityId, Uuid.random(), item, count, x, y, z)

    @Test
    fun spawnedItemsAreFoundWithinTheirRadius() {
        val world = world()
        world.spawnItem(droppedItem(x = 10.0, y = 64.0, z = 10.0))

        assertEquals(1, world.nearbyItems(x = 10.0, y = 64.0, z = 10.0, radius = 1.0).size)
        assertTrue(world.nearbyItems(x = 100.0, y = 64.0, z = 100.0, radius = 1.0).isEmpty())
    }

    @Test
    fun nearbyItemsAreOrderedByDistance() {
        val world = world()
        world.spawnItem(droppedItem(entityId = 1, x = 5.0, y = 64.0, z = 0.0))
        world.spawnItem(droppedItem(entityId = 2, x = 1.0, y = 64.0, z = 0.0))

        val nearby = world.nearbyItems(x = 0.0, y = 64.0, z = 0.0, radius = 10.0)

        assertEquals(listOf(2, 1), nearby.map { it.entityId })
    }

    @Test
    fun removingAnItemMakesItUnavailableToPickUpAgain() {
        val world = world()
        world.spawnItem(droppedItem(entityId = 7))

        val removed = world.removeItem(7)

        assertNotNull(removed)
        assertNull(world.removeItem(7))
        assertTrue(world.nearbyItems(8.5, 64.0, 8.5, 5.0).isEmpty())
    }

    @Test
    fun removingAnUnknownEntityIdIsANoOp() {
        assertNull(world().removeItem(999))
    }
}
