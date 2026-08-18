package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockMovement
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.EntityCollision
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.NoiseWorldGenerator
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.collections.Long2ObjectMap
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.concurrent.Volatile
import kotlin.math.abs
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

    /**
     * A read-only copy of [chunksByPos], replaced under [mutex] whenever a chunk is added or
     * dropped, so a reader can find its chunk without taking the lock at all.
     *
     * Block *reads* off that chunk need no lock either: [org.basaltmc.server.world.PalettedContainer]
     * is written by one thread and read by any number, which is what the send path already relies
     * on. Taking the lock for them cost 635 ns a read against 131 ns without it, and a tick of
     * random block updates does tens of thousands.
     */
    @Volatile
    private var loadedChunks: Long2ObjectMap<Chunk> = snapshotOf(chunksByPos)

    private val generatedEntitiesByKey = mutableMapOf<GeneratedEntityKey, GeneratedWorldEntity>()

    public val chunks: List<Chunk> get() = mutex.withLock { chunksByPos.values.toList() }

    public val loadedChunkCount: Int get() = mutex.withLock { chunksByPos.size }

    /** What every loaded chunk keeps resident, for the timings report's memory breakdown. */
    public data class ChunkMemory(
        val bytes: Long,
        val blockEntities: Int,
        val scheduledTicks: Int,
        val generatedEntities: Int,
    )

    /** One pass under the lock; a /timings call walking ~1500 chunks costs well under a millisecond. */
    public fun chunkMemory(): ChunkMemory =
        mutex.withLock {
            var bytes = 0L
            var blockEntities = 0
            var ticks = 0
            var generated = 0
            for (chunk in chunksByPos.values) {
                bytes += chunk.residentBytes()
                blockEntities += chunk.blockEntityCount
                ticks += chunk.scheduledTickCount
                generated += chunk.generatedEntityCount
            }
            ChunkMemory(bytes, blockEntities, ticks, generated)
        }

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
            if (removed > 0) loadedChunks = snapshotOf(chunksByPos)
            removed
        }

    /** Whether this chunk is already in memory, without generating it: what the queue and the send path poll. */
    public fun isLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = loadedChunks.containsKey(key(chunkX, chunkZ))

    /** The chunk if it is already in memory, or `null`. Never generates, so it is safe on a connection thread. */
    public fun chunkIfLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? = loadedChunks.get(key(chunkX, chunkZ))

    /** Returns the chunk at this position, generating (but not sending or persisting) it first if unseen. */
    public fun getOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val key = key(chunkX, chunkZ)
        mutex.withLock { chunksByPos[key] }?.let { return it }
        val generated = loadOrGenerate(chunkX, chunkZ)
        return mutex.withLock {
            val existing = chunksByPos[key]
            if (existing != null) {
                existing
            } else {
                chunksByPos[key] = generated
                loadedChunks = snapshotOf(chunksByPos)
                // The neighbours were lit without this chunk, so their light let the sky in through
                // where it now stands. parity: vanilla relights a chunk's neighbourhood on load.
                for (offsetZ in -1..1) {
                    for (offsetX in -1..1) {
                        if (offsetX == 0 && offsetZ == 0) continue
                        chunksByPos[key(chunkX + offsetX, chunkZ + offsetZ)]?.invalidateLight()
                    }
                }
                generated
            }
        }
    }

    public fun getBlockState(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val chunk = loadedChunks.get(key(x shr 4, z shr 4)) ?: return null
        return chunk.getBlockStateId(x and 15, y, z and 15)
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
            rememberSculkSensor(x, y, z, stateId)
            true
        }

    /**
     * Where the sculk sensors are, so a walking player does not have to search for them.
     *
     * A vibration from a block event can afford to look around itself; a footstep happens every tick
     * for every player, and cannot. Kept as blocks are set, which covers everything placed or broken
     * while the server runs: a sensor that came with the terrain is not in here until something
     * touches it, so footsteps do not reach the deep dark yet.
     */
    private val sculkSensors = mutableSetOf<Long>()

    private fun rememberSculkSensor(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val path = BlockStateDefinitions.blockOf(stateId)?.path
        val position = blockKey(x, y, z)
        if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
            sculkSensors += position
        } else {
            sculkSensors -= position
        }
    }

    /** Whether any known sensor is within [range] blocks of this point. */
    public fun hasSculkSensorNear(
        x: Double,
        y: Double,
        z: Double,
        range: Int,
    ): Boolean {
        if (sculkSensors.isEmpty()) return false
        return mutex.withLock {
            sculkSensors.any { position ->
                abs(unpackBlockX(position) - x) <= range &&
                    abs(unpackBlockY(position) - y) <= range &&
                    abs(unpackBlockZ(position) - z) <= range
            }
        }
    }

    private fun blockKey(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)

    private fun unpackBlockX(packed: Long): Int = (packed shl 0 shr 38).toInt()

    private fun unpackBlockY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    private fun unpackBlockZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

    /**
     * Queues a fluid tick [delay] ticks from now, doing nothing if the chunk is not loaded.
     *
     * parity: `LevelTicks.schedule`, minus its priorities: only one tick per position and fluid is
     * ever pending, and re-scheduling an existing one keeps the delay already counting down.
     */
    public fun scheduleFluidTick(
        x: Int,
        y: Int,
        z: Int,
        type: Identifier,
        delay: Int,
    ) {
        mutex.withLock {
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock
            if (y < chunk.verticalBounds.minY || y > chunk.verticalBounds.maxY) return@withLock
            chunk.scheduleFluidTick(ChunkScheduledTick(type, x and 15, y, z and 15, delay))
        }
    }

    /** Asks for [type] at these coordinates to be ticked in [delay] ticks. */
    public fun scheduleBlockTick(
        x: Int,
        y: Int,
        z: Int,
        type: Identifier,
        delay: Int,
    ) {
        mutex.withLock {
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock
            if (y < chunk.verticalBounds.minY || y > chunk.verticalBounds.maxY) return@withLock
            chunk.scheduleBlockTick(ChunkScheduledTick(type, x and 15, y, z and 15, delay))
        }
    }

    /** parity: `LevelTicks.hasScheduledTick`, which is what stops a target being re-armed mid-hold. */
    public fun hasScheduledBlockTick(
        x: Int,
        y: Int,
        z: Int,
        type: Identifier,
    ): Boolean =
        mutex.withLock {
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock false
            chunk.scheduledBlockTicks().any {
                it.type == type && it.localX == (x and 15) && it.y == y && it.localZ == (z and 15)
            }
        }

    /** Every fluid tick that comes due this tick, as `(x, y, z)` in world coordinates. */
    public fun dueFluidTicks(): List<IntArray> = dueTicks { chunk, due -> chunk.advanceFluidTicks(due) }

    /** The same for block ticks: a button that has been held down long enough, and later redstone. */
    public fun dueBlockTicks(): List<IntArray> = dueTicks { chunk, due -> chunk.advanceBlockTicks(due) }

    private fun dueTicks(advance: (Chunk, MutableList<ChunkScheduledTick>) -> Unit): List<IntArray> {
        val due = mutableListOf<ChunkScheduledTick>()
        val positions = mutableListOf<IntArray>()
        mutex.withLock {
            for (chunk in chunksByPos.values) {
                if (chunk.scheduledTickCount == 0) continue
                val before = due.size
                advance(chunk, due)
                for (index in before until due.size) {
                    val tick = due[index]
                    positions +=
                        intArrayOf((chunk.chunkX shl 4) + tick.localX, tick.y, (chunk.chunkZ shl 4) + tick.localZ)
                }
            }
        }
        return positions
    }

    /**
     * Runtime identities for the entities of generated chunks within [radiusChunks] of
     * ([centerChunkX], [centerChunkZ]).
     *
     * Bounded because this runs once per player per tick holding [mutex], against the lock the
     * generation workers need. Walked in coordinate order rather than sorted, so an entity's id
     * still depends only on position; [evictClean] is what forgets them, when the chunk goes.
     */
    public fun generatedEntities(
        allocateEntityId: () -> Int,
        centerChunkX: Int,
        centerChunkZ: Int,
        radiusChunks: Int,
    ): List<GeneratedWorldEntity> =
        mutex.withLock {
            val result = mutableListOf<GeneratedWorldEntity>()
            for (chunkX in centerChunkX - radiusChunks..centerChunkX + radiusChunks) {
                for (chunkZ in centerChunkZ - radiusChunks..centerChunkZ + radiusChunks) {
                    val chunkKey = key(chunkX, chunkZ)
                    val chunk = chunksByPos[chunkKey] ?: continue
                    for (index in 0 until chunk.generatedEntityCount) {
                        val entityKey = GeneratedEntityKey(chunkKey, index)
                        result +=
                            generatedEntitiesByKey.getOrPut(entityKey) {
                                GeneratedWorldEntity(allocateEntityId(), Uuid.random(), chunk.generatedEntityAt(index))
                            }
                    }
                }
            }
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
        /**
         * What the stack carries besides its id and count, as a shulker box carries its contents.
         *
         * parity: `ItemEntity` holds a whole `ItemStack`, components and all. Only what a block put
         * there travels this way today, which is enough for a box picked up and put back down.
         */
        public val components: NbtCompound = NbtCompound(),
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

    // --- Summoned entities ---

    /** Reused by [moveItem] so a tick of items allocates nothing. */
    private val movement = DoubleArray(3)

    private val entities = LinkedHashMap<Int, ServerEntity>()

    /** Everything `/summon` put here, alive or just removed and not yet forgotten by clients. */
    public fun entities(): List<ServerEntity> = mutex.withLock { entities.values.toList() }

    public fun entity(entityId: Int): ServerEntity? = mutex.withLock { entities[entityId] }

    public fun spawnEntity(entity: ServerEntity): ServerEntity =
        mutex.withLock {
            entities[entity.entityId] = entity
            entity
        }

    /**
     * The TNT blocks that were lit this tick and still have to become entities.
     *
     * The block itself goes the moment the signal reaches it, but only the caller knows how to
     * allocate an entity id, so what it left behind waits here for one tick at most.
     */
    private val primedTnt = mutableListOf<IntArray>()

    /** parity: `TntBlock.explode`, which spawns the entity where the block stood. */
    public fun primeTnt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        mutex.withLock { primedTnt += intArrayOf(x, y, z) }
    }

    /** Takes what was lit, leaving the queue empty. */
    public fun takePrimedTnt(): List<IntArray> =
        mutex.withLock {
            if (primedTnt.isEmpty()) {
                emptyList()
            } else {
                val lit = primedTnt.toList()
                primedTnt.clear()
                lit
            }
        }

    /** Drops everything marked removed, and returns their ids so clients can be told once. */
    public fun collectRemovedEntities(): List<Int> =
        mutex.withLock {
            val gone = entities.values.filter { it.removed }.map { it.entityId }
            for (id in gone) entities.remove(id)
            gone
        }

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
     * parity: `ItemEntity.tick`: gravity, the move against the real block shapes, air drag, the
     * friction of the block underneath and the half-damped bounce on landing, over the 6000-tick
     * lifetime. Fluids and merging with a neighbouring stack are documented gaps.
     *
     * Deliberately does no I/O and takes no other lock: it runs on the tick thread, which must
     * never touch a socket (see [org.basaltmc.server.runtime.TickLoop]).
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
        // parity: `ItemEntity.getDefaultGravity`, applied before the move rather than after it.
        item.velocityY -= GRAVITY

        val box = Aabb.of(item.x, item.y, item.z, ITEM_WIDTH, ITEM_HEIGHT)
        EntityCollision.collide(this, box, item.velocityX, item.velocityY, item.velocityZ, movement)
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

    /** parity: `getBlockPosBelowThatAffectsMyMovement`, the block whose friction an item feels. */
    private fun blockUnder(item: DroppedItem): Int =
        getBlockState(
            floor(item.x).toInt(),
            floor(item.y - FRICTION_PROBE_DEPTH).toInt(),
            floor(item.z).toInt(),
        ) ?: AIR_STATE_ID

    public companion object {
        private fun snapshotOf(chunks: Map<Long, Chunk>): Long2ObjectMap<Chunk> {
            val copy = Long2ObjectMap<Chunk>(chunks.size * 2 + 4)
            for ((chunkKey, chunk) in chunks) copy.put(chunkKey, chunk)
            return copy
        }

        private fun key(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)

        /** parity: `ItemEntity.LIFETIME`, five minutes at 20 ticks a second. */
        public const val LIFETIME_TICKS: Int = 6000

        /** parity: what a player's own toss gets, so it does not immediately fly back into their inventory. */
        public const val DEFAULT_PICKUP_DELAY: Int = 40

        // parity: `ItemEntity.getDefaultGravity`, `Entity.getAirDrag` and `ItemEntity.tick`'s own
        // `movement.multiply(1.0, -0.5, 1.0)` on landing.
        private const val GRAVITY = 0.04
        private const val AIR_DRAG = 0.98
        private const val BOUNCE_DAMPING = -0.5

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
        private val CAVE_AIR_STATE_ID = Blocks.CAVE_AIR.stateId
        private val VOID_AIR_STATE_ID = Blocks.VOID_AIR.stateId
    }
}
