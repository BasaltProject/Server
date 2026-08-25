package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockMovement
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.EntityCollision
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.abs
import kotlin.math.floor

/**
 * Dropped items in this world, and their physics. Behind the same lock as blocks: a break
 * spawning one and a nearby player picking one up are exactly the same kind of race two players
 * editing blocks are. Shares [WorldState]'s single [mutex] rather than owning its own, since
 * splitting lock granularity would change race outcomes between the tick and save/eviction
 * threads, a real behavior change independent of vanilla parity.
 */
internal class DroppedItemPhysics(
    private val world: WorldState,
    private val mutex: NativeMutex,
) {
    private val droppedItems = mutableListOf<WorldState.DroppedItem>()

    /** Reused by [moveItem] so a tick of items allocates nothing. */
    private val movement = DoubleArray(3)

    /** Adds a newly spawned (or re-added, for a partial pickup) dropped item to this world. */
    public fun spawnItem(dropped: WorldState.DroppedItem): WorldState.DroppedItem =
        mutex.withLock {
            droppedItems.add(dropped)
            dropped
        }

    /** Every dropped item in this world, as a snapshot safe to iterate outside the lock. */
    public fun items(): List<WorldState.DroppedItem> = mutex.withLock { droppedItems.toList() }

    /** A snapshot of every dropped item within [radius] blocks of a point, nearest first. */
    public fun nearbyItems(
        x: Double,
        y: Double,
        z: Double,
        radius: Double,
    ): List<WorldState.DroppedItem> =
        mutex.withLock {
            val radiusSquared = radius * radius
            droppedItems
                .filter { squaredDistance(it, x, y, z) <= radiusSquared }
                .sortedBy { squaredDistance(it, x, y, z) }
        }

    private fun squaredDistance(
        item: WorldState.DroppedItem,
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        val dx = item.x - x
        val dy = item.y - y
        val dz = item.z - z
        return dx * dx + dy * dy + dz * dz
    }

    /** Removes and returns the dropped item with this entity id, or `null` if it is already gone (picked up, despawned). */
    public fun removeItem(entityId: Int): WorldState.DroppedItem? =
        mutex.withLock {
            val index = droppedItems.indexOfFirst { it.entityId == entityId }
            if (index < 0) null else droppedItems.removeAt(index)
        }

    /**
     * One tick of item physics for the whole world, returning the entity ids that despawned.
     *
     * parity: `ItemEntity.tick`: gravity, the move against the real block shapes, air drag, the
     * friction of the block underneath and the half-damped bounce on landing, over the 6000-tick
     * lifetime, and the merge with a neighbouring stack. Floating on a fluid is a documented gap.
     *
     * Deliberately does no I/O and takes no other lock: it runs on the tick thread, which must
     * never touch a socket (see [org.basaltmc.server.runtime.TickLoop]).
     */
    public fun tickItems(region: Region? = null): List<Int> =
        mutex.withLock {
            val despawned = mutableListOf<Int>()
            val iterator = droppedItems.iterator()
            while (iterator.hasNext()) {
                val item = iterator.next()
                // An item belongs to whoever owns the ground it is lying on. With the world in one
                // piece that is everybody, and this is always true.
                if (region != null && !region.ownsPosition(item.x, item.z)) continue
                item.age++
                if (item.pickupDelay > 0) item.pickupDelay--
                if (item.age >= WorldState.LIFETIME_TICKS) {
                    despawned += item.entityId
                    iterator.remove()
                    continue
                }
                if (!item.resting) moveItem(item)
            }
            // parity: `ItemEntity.mergeWithNeighbours`, asked every other tick while an item is
            // moving and every two seconds once it has settled. Without it a broken tree leaves six
            // stacks of one log on the floor where vanilla leaves one.
            val absorbed = HashSet<Int>()
            for (item in droppedItems.toList()) {
                if (item.entityId in absorbed) continue
                val interval = if (item.resting) MERGE_RESTING_INTERVAL else MERGE_MOVING_INTERVAL
                if (item.age % interval != 0) continue
                mergeWithNeighbours(item, despawned, absorbed)
            }
            despawned
        }

    /**
     * Pours whatever is close enough into [item], and marks what it emptied as gone.
     *
     * parity: `ItemEntity.tryToMerge`: same item, same components, and the two counts have to fit
     * in one stack. Called with [mutex] already held.
     */
    private fun mergeWithNeighbours(
        item: WorldState.DroppedItem,
        despawned: MutableList<Int>,
        absorbed: MutableSet<Int>,
    ) {
        if (item.count >= MAX_STACK) return
        val iterator = droppedItems.iterator()
        while (iterator.hasNext()) {
            val other = iterator.next()
            if (other === item || other.item != item.item || other.count <= 0) continue
            if (!sameComponents(other.components, item.components)) continue
            if (item.count + other.count > MAX_STACK) continue
            if (abs(other.x - item.x) > MERGE_RANGE || abs(other.z - item.z) > MERGE_RANGE) continue
            if (abs(other.y - item.y) > MERGE_HEIGHT) continue
            item.count += other.count
            item.countChanged = true
            // parity: the merged stack keeps the younger age and the longer pickup delay.
            item.age = minOf(item.age, other.age)
            item.pickupDelay = maxOf(item.pickupDelay, other.pickupDelay)
            despawned += other.entityId
            absorbed += other.entityId
            iterator.remove()
            if (item.count >= MAX_STACK) return
        }
    }

    /** Integrates one item for one tick. Called with [mutex] already held. */
    private fun moveItem(item: WorldState.DroppedItem) {
        // parity: `ItemEntity.getDefaultGravity`, applied before the move rather than after it.
        item.velocityY -= GRAVITY

        val box = Aabb.of(item.x, item.y, item.z, ITEM_WIDTH, ITEM_HEIGHT)
        EntityCollision.collide(world, box, item.velocityX, item.velocityY, item.velocityZ, movement)
        val movedX = movement[0]
        val movedY = movement[1]
        val movedZ = movement[2]
        item.x += movedX
        item.y += movedY
        item.z += movedZ

        // parity: `Entity.restituteMovementAfterCollisions` with no bounce: a stopped axis is zeroed.
        val onGround = movedY != item.velocityY && item.velocityY < 0.0
        val bounciness = if (onGround) BlockMovement.bouncinessOf(blockUnder(item)) * ITEM_BOUNCE_SHARE else 0f
        if (movedX != item.velocityX) item.velocityX = 0.0
        if (movedZ != item.velocityZ) item.velocityZ = 0.0
        if (movedY != item.velocityY) {
            item.velocityY = if (bounciness > 0f) -item.velocityY * bounciness else 0.0
        }

        // parity: `ItemEntity.tick`'s own drag: the air takes its share every tick, and the block
        // underneath takes its friction on top of that.
        var horizontalDrag = AIR_DRAG
        if (onGround) horizontalDrag *= BlockMovement.frictionOf(blockUnder(item)).toDouble()
        item.velocityX *= horizontalDrag
        item.velocityY *= AIR_DRAG
        item.velocityZ *= AIR_DRAG
        if (onGround && item.velocityY < 0.0) item.velocityY *= BOUNCE_DAMPING

        // parity: the check that lets vanilla skip a settled item's physics, plus the vertical half
        // of it. Here it also stops the movement packets, which is what makes a floor covered in
        // drops free to look at; an item still bouncing off slime is not settled and keeps moving.
        item.resting =
            onGround &&
            item.velocityX * item.velocityX + item.velocityZ * item.velocityZ <= RESTING_SPEED_SQUARED &&
            abs(item.velocityY) <= RESTING_SPEED
        if (item.resting) {
            item.velocityX = 0.0
            item.velocityY = 0.0
            item.velocityZ = 0.0
        }
    }

    /** Two stacks only merge when they carry the same extras, which for now means neither carries any. */
    private fun sameComponents(
        a: NbtCompound,
        b: NbtCompound,
    ): Boolean = a.keys.isEmpty() && b.keys.isEmpty()

    /** parity: `getBlockPosBelowThatAffectsMyMovement`, the block whose friction an item feels. */
    private fun blockUnder(item: WorldState.DroppedItem): Int =
        world.getBlockState(
            floor(item.x).toInt(),
            floor(item.y - FRICTION_PROBE_DEPTH).toInt(),
            floor(item.z).toInt(),
        ) ?: AIR_STATE_ID

    private companion object {
        // parity: `ItemEntity.getDefaultGravity`, `Entity.getAirDrag` and `ItemEntity.tick`'s own
        // `movement.multiply(1.0, -0.5, 1.0)` on landing.
        private const val GRAVITY = 0.04
        private const val AIR_DRAG = 0.98
        private const val BOUNCE_DAMPING = -0.5

        /** parity: `mergeWithNeighbours`' own box, half a block out sideways and none at all up. */
        private const val MERGE_RANGE = 0.5
        private const val MERGE_HEIGHT = 0.25
        private const val MERGE_MOVING_INTERVAL = 2
        private const val MERGE_RESTING_INTERVAL = 40

        /** The stack this build fills, the same one the inventory works in. */
        private const val MAX_STACK = 64

        /** parity: `EntityType.ITEM`'s own box, a quarter of a block each way. */
        private const val ITEM_WIDTH = 0.25
        private const val ITEM_HEIGHT = 0.25

        /** parity: `Entity.getBlockBounciness`, which gives anything that is not alive four fifths. */
        private const val ITEM_BOUNCE_SHARE = 0.8f

        /** parity: `getBlockPosBelowThatAffectsMyMovement`, which looks half a block under the feet. */
        private const val FRICTION_PROBE_DEPTH = 0.5

        /** parity: the 1.0E-5 horizontal speed under which `ItemEntity.tick` skips its own physics. */
        private const val RESTING_SPEED_SQUARED = 1.0e-5

        /** And the vertical speed that counts as none, so a bounce is never mistaken for a rest. */
        private const val RESTING_SPEED = 1.0e-3

        private val AIR_STATE_ID = Blocks.AIR.stateId
    }
}
