package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
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

    /** A floor of one named block, so friction and bounce can be told apart from stone's. */
    private fun worldFlooredWith(block: String): WorldState {
        val chunk = Chunk(0, 0)
        val stateId =
            checkNotNull(
                org.basaltmc.server.registry.BlockStateDefinitions
                    .stateIdOf(Identifier.vanilla(block), emptyMap()),
            )
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 64, z, stateId)
        }
        return WorldState(listOf(chunk))
    }

    private fun slideOn(block: String): Double {
        val world = worldFlooredWith(block)
        val item = world.spawnItem(droppedItem(x = 4.5, y = 65.0, z = 8.5))
        item.velocityX = 0.5
        repeat(20) { world.tickItems() }
        return item.x - 4.5
    }

    /** parity: `ItemEntity.tick` multiplies the drag by the friction of the block underneath. */
    @Test
    fun anItemSlidesFurtherOnIce() {
        val onStone = slideOn("stone")
        val onIce = slideOn("ice")

        assertTrue(onStone > 0.0, "it slid at all: $onStone")
        assertTrue(onIce > onStone * 2, "ice should carry it much further: $onIce against $onStone")
    }

    /** parity: `Entity.restituteMovementAfterCollisions`, and the four fifths a non-living entity keeps. */
    @Test
    fun anItemBouncesOffSlime() {
        val world = worldFlooredWith("slime_block")
        val item = world.spawnItem(droppedItem(x = 8.5, y = 68.0, z = 8.5))

        var highestBounce = 0.0
        var landed = false
        repeat(60) {
            world.tickItems()
            if (item.velocityY > 0.0) {
                landed = true
                highestBounce = maxOf(highestBounce, item.velocityY)
            }
        }

        assertTrue(landed, "it never bounced off the slime")
        assertTrue(highestBounce > 0.1, "the bounce was barely there: $highestBounce")
    }

    /** parity: `Entity.collide`, which stops a sideways move dead against a wall. */
    @Test
    fun aWallStopsAnItemSliding() {
        val world = worldWithFloor()
        checkNotNull(world.chunkIfLoaded(0, 0)).setBlockStateId(10, 65, 8, Blocks.STONE.stateId)
        val item = world.spawnItem(droppedItem(x = 8.5, y = 65.0, z = 8.5))
        item.velocityX = 0.5

        repeat(20) { world.tickItems() }

        assertTrue(item.x < 10.0, "it went through the wall: ${item.x}")
        assertEquals(0.0, item.velocityX, "and the wall took its speed")
    }
}
