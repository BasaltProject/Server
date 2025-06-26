package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `ItemEntity.mergeWithNeighbours`, which is why a broken tree leaves one stack and not six. */
@OptIn(ExperimentalUuidApi::class)
class ItemMergeTest {
    private fun world(): WorldState = WorldState(listOf(Chunk(0, 0)))

    private fun drop(
        world: WorldState,
        id: Int,
        item: String,
        count: Int,
        x: Double,
        z: Double,
    ) = world.spawnItem(
        WorldState
            .DroppedItem(
                entityId = id,
                uuid = Uuid.random(),
                item = Identifier.vanilla(item),
                count = count,
                x = x,
                y = 64.0,
                z = z,
            ).also { it.resting = true },
    )

    @Test
    fun twoStacksOfTheSameThingBecomeOne() {
        val world = world()
        drop(world, 1, "oak_log", 3, 8.5, 8.5)
        drop(world, 2, "oak_log", 4, 8.7, 8.5)

        // parity: a settled item only looks around every two seconds, so the merge is not immediate.
        val despawned = mutableListOf<Int>()
        repeat(MERGE_WINDOW) { despawned += world.tickItems() }
        assertTrue(2 in despawned, "the second stack is gone")
        val left = world.items().single()
        assertEquals(7, left.count)
        assertTrue(left.countChanged)
    }

    @Test
    fun differentItemsAreLeftAlone() {
        val world = world()
        drop(world, 1, "oak_log", 3, 8.5, 8.5)
        drop(world, 2, "stone", 4, 8.7, 8.5)

        repeat(MERGE_WINDOW) { assertTrue(world.tickItems().isEmpty()) }
        assertEquals(2, world.items().size)
    }

    @Test
    fun stacksTooFarApartAreLeftAlone() {
        val world = world()
        drop(world, 1, "oak_log", 3, 8.5, 8.5)
        drop(world, 2, "oak_log", 4, 10.0, 8.5)

        repeat(MERGE_WINDOW) { assertTrue(world.tickItems().isEmpty()) }
        assertEquals(2, world.items().size)
    }

    @Test
    fun nothingMergesPastAFullStack() {
        val world = world()
        drop(world, 1, "oak_log", 60, 8.5, 8.5)
        drop(world, 2, "oak_log", 10, 8.6, 8.5)

        repeat(MERGE_WINDOW) { assertTrue(world.tickItems().isEmpty()) }
        assertEquals(2, world.items().size)
    }

    private companion object {
        /** Long enough for the two-second look a settled item takes. */
        const val MERGE_WINDOW = 45
    }
}
