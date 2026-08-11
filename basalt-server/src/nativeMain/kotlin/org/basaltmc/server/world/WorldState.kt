package org.basaltmc.server.world

import org.basaltmc.io.NativeMutex
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkGeneratedEntity
import org.basaltmc.world.NoiseWorldGenerator
import kotlin.math.floor
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Shared view over the loaded chunks, with one coarse lock over both block writes and the chunk
 * set: sessions run a thread each. Per-region locking waits on the scheduler ADR.
 *
 * [getOrGenerateChunk] releases [mutex] around [loadOrGenerate], which can take over a second and
 * would freeze every other player behind the slowest chunk. Racing threads both generate and one
 * result is discarded: generation is deterministic, so that beats serializing every load.
 *
 * [loadOrGenerate] must read the region files first, or a visited chunk comes back regenerated
 * with the player's edits gone.
 */
@OptIn(ExperimentalUuidApi::class)
public class WorldState(
    initialChunks: List<Chunk>,
    private val loadOrGenerate: (chunkX: Int, chunkZ: Int) -> Chunk = NoiseWorldGenerator::generate,
) {
    private val mutex = NativeMutex()

    // Keyed by packed coordinates, not by `Pair`. A Pair key allocates an object and boxes both
    // ints on *every* lookup, and these are the most called methods in the server: the login path
    // alone polls hundreds of positions per millisecond.
    private val chunksByPos =
        HashMap<Long, Chunk>().apply {
            for (chunk in initialChunks) this[key(chunk.chunkX, chunk.chunkZ)] = chunk
        }

    private val generatedEntitiesByKey = mutableMapOf<GeneratedEntityKey, GeneratedWorldEntity>()

    public val chunks: List<Chunk> get() = mutex.withLock { chunksByPos.values.toList() }

    public val loadedChunkCount: Int get() = mutex.withLock { chunksByPos.size }

    /**
     * Only the chunks holding something the region files do not, which is all a save has to write.
     *
     * Snapshots under the lock and filters outside it. Every generation worker needs this same lock
     * twice per chunk, so holding it across a scan of everything loaded, from a single thread
     * competing with eight, is how that thread ends up never being scheduled at all.
     */
    public fun dirtyChunks(): List<Chunk> = snapshot().filter { it.isDirty }

    /** Every loaded chunk [shouldKeep] rejects. Filtered outside the lock, for the reason [dirtyChunks] gives. */
    public fun evictionCandidates(shouldKeep: (chunkX: Int, chunkZ: Int) -> Boolean): List<Chunk> =
        snapshot().filter { !shouldKeep(it.chunkX, it.chunkZ) }

    private fun snapshot(): List<Chunk> = mutex.withLock { chunksByPos.values.toList() }

    /** How many of [positions] are still missing, counted in one pass under one lock acquisition. */
    public fun countMissing(positions: List<Pair<Int, Int>>): Int =
        mutex.withLock { positions.count { key(it.first, it.second) !in chunksByPos } }

    /**
     * Drops [chunks] from memory, skipping any that became dirty again since the caller saved them:
     * evicting a chunk whose changes are not on disk throws away whatever the player built in it,
     * and it will simply come up for eviction again on the next sweep.
     */
    public fun evictClean(chunks: List<Chunk>): Int =
        mutex.withLock {
            var removed = 0
            for (chunk in chunks) {
                if (chunk.isDirty) continue
                val chunkKey = key(chunk.chunkX, chunk.chunkZ)
                if (chunksByPos.remove(chunkKey) != null) {
                    generatedEntitiesByKey.keys.removeAll { it.chunkKey == chunkKey }
                    removed++
                }
            }
            removed
        }

    /** Whether this chunk is already in memory, without generating it: what the queue and the send path poll. */
    public fun isLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = mutex.withLock { key(chunkX, chunkZ) in chunksByPos }

    /** The chunk if it is already in memory, or `null`. Never generates, so it is safe on a connection thread. */
    public fun chunkIfLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? = mutex.withLock { chunksByPos[key(chunkX, chunkZ)] }

    /** Returns the chunk at this position, generating (but not sending or persisting) it first if unseen. */
    public fun getOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val key = key(chunkX, chunkZ)
        mutex.withLock { chunksByPos[key] }?.let { return it }
        val generated = loadOrGenerate(chunkX, chunkZ)
        return mutex.withLock { chunksByPos.getOrPut(key) { generated } }
    }

    public fun getBlockState(
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        mutex.withLock {
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock null
            chunk.getBlockStateId(x and 15, y, z and 15)
        }

    /** Returns `true` if the position falls inside a loaded chunk (and the block was set), `false` otherwise. */
    public fun setBlockState(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean =
        mutex.withLock {
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock false
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
            true
        }

    /** Runtime identities for immutable entities emitted by every currently loaded generated chunk. */
    public fun generatedEntities(allocateEntityId: () -> Int): List<GeneratedWorldEntity> =
        mutex.withLock {
            val visibleKeys = mutableSetOf<GeneratedEntityKey>()
            val result = mutableListOf<GeneratedWorldEntity>()
            val chunks = chunksByPos.values.sortedWith(compareBy(Chunk::chunkX, Chunk::chunkZ))
            for (chunk in chunks) {
                val chunkKey = key(chunk.chunkX, chunk.chunkZ)
                for ((index, descriptor) in chunk.generatedEntities().withIndex()) {
                    val entityKey = GeneratedEntityKey(chunkKey, index)
                    visibleKeys += entityKey
                    result +=
                        generatedEntitiesByKey.getOrPut(entityKey) {
                            GeneratedWorldEntity(allocateEntityId(), Uuid.random(), descriptor)
                        }
                }
            }
            generatedEntitiesByKey.keys.removeAll { it !in visibleKeys }
            result
        }

    public data class GeneratedWorldEntity(
        val entityId: Int,
        val uuid: Uuid,
        val descriptor: ChunkGeneratedEntity,
    )

    private data class GeneratedEntityKey(
        val chunkKey: Long,
        val index: Int,
    )

    // Dropped items in this world. Behind the same lock as blocks: a break spawning one and a
    // nearby player picking one up are exactly the same kind of race two players editing blocks
    // are. [tickItems] moves them; connection threads only read the result and put it on the wire.
    private val droppedItems = mutableListOf<DroppedItem>()

    /**
     * One item lying in (or falling through) the world.
     *
     * parity: `ItemEntity`. Position and velocity are mutable because [tickItems] integrates them
     * once per server tick, on whichever thread owns the tick, never on a connection's. What each
     * client is told about them is a separate concern (see `ClientSession`'s own item tracking):
     * this class only knows where the item is, not who can see it.
     */
    public class DroppedItem(
        public val entityId: Int,
        public val uuid: Uuid,
        public val item: Identifier,
        public var count: Int,
        public var x: Double,
        public var y: Double,
        public var z: Double,
        public var velocityX: Double = 0.0,
        public var velocityY: Double = 0.0,
        public var velocityZ: Double = 0.0,
        /** Ticks before anyone may pick this up. parity: 40 for a player's own toss, so it does not fly straight back. */
        public var pickupDelay: Int = DEFAULT_PICKUP_DELAY,
        /** Ticks lived. parity: `ItemEntity` despawns at [LIFETIME_TICKS]. */
        public var age: Int = 0,
    ) {
        /** Whether this settled: once true the server stops sending movement for it, as vanilla does. */
        public var resting: Boolean = false
            internal set

        public val canBePickedUp: Boolean get() = pickupDelay <= 0
    }

    /** Adds a newly spawned (or re-added, for a partial pickup) dropped item to this world. */
    public fun spawnItem(dropped: DroppedItem): DroppedItem =
        mutex.withLock {
            droppedItems.add(dropped)
            dropped
        }

    /** Every dropped item in this world, as a snapshot safe to iterate outside the lock. */
    public fun items(): List<DroppedItem> = mutex.withLock { droppedItems.toList() }

    /** A snapshot of every dropped item within [radius] blocks of a point, nearest first. */
    public fun nearbyItems(
        x: Double,
        y: Double,
        z: Double,
        radius: Double,
    ): List<DroppedItem> =
        mutex.withLock {
            val radiusSquared = radius * radius
            droppedItems
                .filter { squaredDistance(it, x, y, z) <= radiusSquared }
                .sortedBy { squaredDistance(it, x, y, z) }
        }

    private fun squaredDistance(
        item: DroppedItem,
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
    public fun removeItem(entityId: Int): DroppedItem? =
        mutex.withLock {
            val index = droppedItems.indexOfFirst { it.entityId == entityId }
            if (index < 0) null else droppedItems.removeAt(index)
        }

    /**
     * One tick of item physics for the whole world, returning the entity ids that despawned.
     *
     * parity: `ItemEntity.tick`, minus what this build has no model for. Gravity, air drag, ground
     * friction, the half-damped vertical bounce and the 6000-tick lifetime are vanilla's own
     * numbers, read off the real 26.2 jar rather than guessed. Fluids, block friction other than
     * the default, entity collision and merging with neighbouring stacks are documented gaps.
     *
     * Deliberately does no I/O and takes no other lock: it runs on the tick thread, which must
     * never touch a socket (see [org.basaltmc.server.TickLoop]).
     */
    public fun tickItems(): List<Int> =
        mutex.withLock {
            val despawned = mutableListOf<Int>()
            val iterator = droppedItems.iterator()
            while (iterator.hasNext()) {
                val item = iterator.next()
                item.age++
                if (item.pickupDelay > 0) item.pickupDelay--
                if (item.age >= LIFETIME_TICKS) {
                    despawned += item.entityId
                    iterator.remove()
                    continue
                }
                if (!item.resting) moveItem(item)
            }
            despawned
        }

    /** Integrates one item for one tick. Called with [mutex] already held. */
    private fun moveItem(item: DroppedItem) {
        item.velocityY -= GRAVITY

        val nextX = item.x + item.velocityX
        val nextY = item.y + item.velocityY
        val nextZ = item.z + item.velocityZ

        // Only the vertical sweep is resolved against blocks: after friction the horizontal drift
        // is small enough that walking an item into a wall is not what this build gets wrong.
        if (item.velocityY < 0 && isSolidAt(nextX, nextY, nextZ)) {
            item.x = nextX
            item.z = nextZ
            item.y = floor(nextY) + 1.0

            // A gentle landing settles rather than bouncing. It has to: gravity adds 0.04 per tick
            // and a half-damped bounce gives back less than that once the fall is slow, so bouncing
            // unconditionally never converges and the item jitters on the floor forever, sending a
            // movement packet every tick for the rest of its five minutes.
            if (item.velocityY > -BOUNCE_THRESHOLD) {
                item.velocityX = 0.0
                item.velocityY = 0.0
                item.velocityZ = 0.0
                item.resting = true
                return
            }

            item.velocityY *= BOUNCE_DAMPING
            item.velocityX *= GROUND_FRICTION
            item.velocityZ *= GROUND_FRICTION
        } else {
            item.x = nextX
            item.y = nextY
            item.z = nextZ
        }

        item.velocityX *= AIR_DRAG
        item.velocityY *= AIR_DRAG
        item.velocityZ *= AIR_DRAG
    }

    /** Whether the block containing this point stops an item. Called with [mutex] already held. */
    private fun isSolidAt(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val blockX = kotlin.math.floor(x).toInt()
        val blockY = kotlin.math.floor(y).toInt()
        val blockZ = kotlin.math.floor(z).toInt()
        val chunk = chunksByPos[key(blockX shr 4, blockZ shr 4)] ?: return true
        val stateId = chunk.getBlockStateId(blockX and 15, blockY, blockZ and 15)
        return stateId != AIR_STATE_ID && stateId != CAVE_AIR_STATE_ID && stateId != VOID_AIR_STATE_ID
    }

    public companion object {
        private fun key(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)

        /** parity: `ItemEntity.LIFETIME`, five minutes at 20 ticks a second. */
        public const val LIFETIME_TICKS: Int = 6000

        /** parity: what a player's own toss gets, so it does not immediately fly back into their inventory. */
        public const val DEFAULT_PICKUP_DELAY: Int = 40

        // parity: ItemEntity.getDefaultGravity, Entity.getAirDrag, the default block friction, and
        // ItemEntity.tick's own `movement.multiply(1.0, -0.5, 1.0)` on landing.
        private const val GRAVITY = 0.04
        private const val AIR_DRAG = 0.98
        private const val GROUND_FRICTION = 0.6
        private const val BOUNCE_DAMPING = -0.5

        /** A landing slower than this settles instead of bouncing. Above one tick of gravity, so it always converges. */
        private const val BOUNCE_THRESHOLD = 0.1

        private val AIR_STATE_ID = Blocks.AIR.stateId
        private val CAVE_AIR_STATE_ID = Blocks.CAVE_AIR.stateId
        private val VOID_AIR_STATE_ID = Blocks.VOID_AIR.stateId
    }
}
