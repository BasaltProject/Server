package org.basaltmc.server.world

import org.basaltmc.common.Identifier
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
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

    /** A world with a solid floor at y=64, so a dropped item has something to land on. */
    private fun worldWithFloor(): WorldState {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.STONE.stateId)
        }
        return WorldState(listOf(chunk))
    }

    @Test
    fun aDroppedItemFallsUnderGravity() {
        val world = worldWithFloor()
        world.spawnItem(droppedItem(x = 8.5, y = 80.0, z = 8.5))

        world.tickItems()

        val item = world.items().single()
        assertTrue(item.y < 80.0, "the item should have fallen")
        assertTrue(item.velocityY < 0.0, "gravity should be pulling it down")
    }

    @Test
    fun aFallingItemSettlesOnTheFloorAndStopsMoving() {
        val world = worldWithFloor()
        world.spawnItem(droppedItem(x = 8.5, y = 70.0, z = 8.5))

        // Gravity adds more speed per tick than a half-damped bounce gives back, so an item that
        // bounced unconditionally would never converge and would send a movement packet every tick
        // for the rest of its life. It has to actually come to rest.
        repeat(200) { world.tickItems() }

        val item = world.items().single()
        assertTrue(item.resting, "the item never settled, so it would stream movement forever")
        assertEquals(65.0, item.y, "it should be resting on top of the floor at y=64")
        assertEquals(0.0, item.velocityY)
    }

    @Test
    fun anItemDespawnsAfterItsLifetime() {
        val world = worldWithFloor()
        world.spawnItem(droppedItem(entityId = 3, x = 8.5, y = 66.0, z = 8.5))

        repeat(WorldState.LIFETIME_TICKS - 1) { world.tickItems() }
        assertEquals(1, world.items().size, "it must survive right up to its last tick")

        val despawned = world.tickItems()

        assertEquals(listOf(3), despawned)
        assertTrue(world.items().isEmpty())
    }

    @Test
    fun aFreshlyTossedItemCannotBePickedUpImmediately() {
        val world = worldWithFloor()
        val item = world.spawnItem(droppedItem(y = 66.0))

        // parity: a player's own toss waits out `ItemEntity.pickupDelay`, or it would fly straight
        // back into the inventory it was thrown from.
        assertFalse(item.canBePickedUp)
        repeat(WorldState.DEFAULT_PICKUP_DELAY) { world.tickItems() }
        assertTrue(item.canBePickedUp)
    }
}
