package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockMovement
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.CubeMobs
import org.basaltmc.server.runtime.entity.EntityCollision
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.sched.Owner
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedBlockEntityWrite
import org.basaltmc.server.world.ChunkGeneratedBlockTick
import org.basaltmc.server.world.ChunkGeneratedBlockWrite
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
 * Shared view over the loaded chunks.
 *
 * The owner writes blocks, entities and items without any lock: it is the only writer, and it finds
 * its chunk in the same lock-free snapshot a read uses. The lock is left for the two threads that
 * are deliberately not the owner, the periodic save and the unload sweep, which are slow I/O.
 *
 * A known hole: saving encodes a chunk while the owner may be writing blocks into it.
 *
 * [loadOrGenerate] must read the region files before generating, or a visited chunk comes back
 * regenerated with the player's edits gone.
 */
@OptIn(ExperimentalUuidApi::class)
public class WorldState(
    initialChunks: List<Chunk>,
    private val loadOrGenerate: (chunkX: Int, chunkZ: Int) -> Chunk = NoiseWorldGenerator::generate,
) {
    private val mutex = NativeMutex()

    /**
     * Who owns this state, once somebody does.
     *
     * Set by [LoadedWorld]; null for the bare `WorldState` a test builds, which is single threaded
     * by construction. What it buys is the check in [mine]: with one region an access from the
     * wrong thread works by luck, and luck is what stops working the day regions are turned on.
     */
    public var owner: Owner? = null

    /**
     * How many regions this world is in, which decides whether [mine] can say anything useful.
     *
     * Set by [LoadedWorld]; one for a world built on its own.
     */
    public var regionCount: () -> Int = { 1 }

    /**
     * Refuses an access from anywhere but the owner, and only while [Ownership.strict] is on.
     *
     * Skipped while a world is split: [owner] is the first region's, and a block out in the second
     * one belongs to that one instead, so the check would refuse what is in fact correct. Checking
     * per position is what closes it, and it needs the position at every call site rather than at
     * some of them; until then the enforcement is what it was designed for, which is the default
     * single-region mode.
     */
    private fun mine(what: String) {
        if (regionCount() > 1) return
        owner?.check(what)
    }

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
    private val pendingGeneratedBlockWrites = HashMap<Long, MutableList<ChunkGeneratedBlockWrite>>()
    private val pendingGeneratedBlockEntityWrites = HashMap<Long, MutableList<ChunkGeneratedBlockEntityWrite>>()
    private val pendingGeneratedBlockTicks = HashMap<Long, MutableList<ChunkGeneratedBlockTick>>()

    init {
        // Tests and embedders may seed a world with detached generated chunks. Production starts
        // empty, but those chunks still deserve the same cross-border handoff as [install].
        for (chunk in initialChunks) {
            applyPendingGeneratedBlockWrites(chunk)
            applyPendingGeneratedBlockEntityWrites(chunk)
            applyPendingGeneratedBlockTicks(chunk)
            deliverGeneratedBlockWrites(chunk.takeGeneratedBlockWrites())
            deliverGeneratedBlockEntityWrites(chunk.takeGeneratedBlockEntityWrites())
            deliverGeneratedBlockTicks(chunk.takeGeneratedBlockTicks())
        }
        loadedChunks = snapshotOf(chunksByPos)
    }

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

    /** Loaded chunks carrying entities from the last save, which have to be rewritten when they move. */
    public fun chunksWithStashedEntities(): List<Chunk> = snapshot().filter { it.savedEntities().isNotEmpty() }

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
            if (removed > 0) {
                loadedChunks = snapshotOf(chunksByPos)
                chunkVersion.addAndGet(1)
            }
            removed
        }

    /**
     * Every loaded chunk as one packed key, for whoever has to reason about the shape of the world
     * rather than about its contents: the region scheduler works out what touches what from this.
     */
    public fun loadedChunkKeys(): List<Long> = mutex.withLock { chunksByPos.keys.toList() }

    /**
     * Bumped whenever a chunk arrives or goes.
     *
     * What the region scheduler asks before working out the regions again: the shape of a world
     * only changes when its chunks do, and walking every loaded chunk on a tick where none of them
     * moved was pure cost.
     */
    public val chunkSetVersion: Long get() = chunkVersion.value

    private val chunkVersion = kotlin.concurrent.AtomicLong(0)

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
        loadedChunks.get(key(chunkX, chunkZ))?.let { return it }
        val generated = generateDetached(chunkX, chunkZ) ?: return getOrGenerateChunk(chunkX, chunkZ)
        return install(generated)
    }

    /**
     * Reads or generates one chunk without putting it anywhere.
     *
     * The expensive half, and the half that touches nothing shared: a generation worker runs this
     * on its own thread and then hands the result to the owner with [installLater]. Answers null
     * when the chunk turned up in the meantime, so the caller reads it instead.
     */
    public fun generateDetached(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? {
        if (loadedChunks.containsKey(key(chunkX, chunkZ))) return null
        return loadOrGenerate(chunkX, chunkZ)
    }

    /**
     * Puts a generated chunk into the world. The owner's job, because the set of chunks is its
     * state; answers whichever chunk ends up in the world, which is the one already there if two
     * workers raced on the same position.
     */
    public fun install(chunk: Chunk): Chunk {
        if (installOwner == null) mine("The set of chunks")
        val chunkX = chunk.chunkX
        val chunkZ = chunk.chunkZ
        val key = key(chunkX, chunkZ)
        return mutex.withLock {
            val existing = chunksByPos[key]
            if (existing != null) {
                existing
            } else {
                chunksByPos[key] = chunk
                applyPendingGeneratedBlockWrites(chunk)
                applyPendingGeneratedBlockEntityWrites(chunk)
                applyPendingGeneratedBlockTicks(chunk)
                deliverGeneratedBlockWrites(chunk.takeGeneratedBlockWrites())
                deliverGeneratedBlockEntityWrites(chunk.takeGeneratedBlockEntityWrites())
                deliverGeneratedBlockTicks(chunk.takeGeneratedBlockTicks())
                loadedChunks = snapshotOf(chunksByPos)
                chunkVersion.addAndGet(1)
                pendingEntities += chunk.takeSavedEntities()
                // The neighbours were lit without this chunk, so their light let the sky in through
                // where it now stands. parity: vanilla relights a chunk's neighbourhood on load.
                for (offsetZ in -1..1) {
                    for (offsetX in -1..1) {
                        if (offsetX == 0 && offsetZ == 0) continue
                        chunksByPos[key(chunkX + offsetX, chunkZ + offsetZ)]?.invalidateLight()
                    }
                }
                chunk
            }
        }
    }

    /**
     * Delivers decorations that crossed out of their source chunk without ever wrapping them back
     * into it. A target that has not arrived yet keeps the write until its own install.
     */
    private fun deliverGeneratedBlockWrites(writes: List<ChunkGeneratedBlockWrite>) {
        for (write in writes) {
            val targetKey = key(write.x shr 4, write.z shr 4)
            val target = chunksByPos[targetKey]
            if (target == null) {
                pendingGeneratedBlockWrites.getOrPut(targetKey, ::mutableListOf) += write
            } else {
                applyGeneratedBlockWrite(target, write)
            }
        }
    }

    private fun applyPendingGeneratedBlockWrites(chunk: Chunk) {
        val writes = pendingGeneratedBlockWrites.remove(key(chunk.chunkX, chunk.chunkZ)) ?: return
        for (write in writes) applyGeneratedBlockWrite(chunk, write)
    }

    private fun applyGeneratedBlockWrite(
        chunk: Chunk,
        write: ChunkGeneratedBlockWrite,
    ) {
        if (write.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        val localX = write.x and 15
        val localZ = write.z and 15
        val current = chunk.getBlockStateId(localX, write.y, localZ)
        if (current == write.expectedStateId) {
            chunk.setBlockStateId(localX, write.y, localZ, write.stateId)
        }
    }

    private fun deliverGeneratedBlockEntityWrites(writes: List<ChunkGeneratedBlockEntityWrite>) {
        for (write in writes) {
            val targetKey = key(write.x shr 4, write.z shr 4)
            val target = chunksByPos[targetKey]
            if (target == null) {
                pendingGeneratedBlockEntityWrites.getOrPut(targetKey, ::mutableListOf) += write
            } else {
                applyGeneratedBlockEntityWrite(target, write)
            }
        }
    }

    private fun applyPendingGeneratedBlockEntityWrites(chunk: Chunk) {
        val writes = pendingGeneratedBlockEntityWrites.remove(key(chunk.chunkX, chunk.chunkZ)) ?: return
        for (write in writes) applyGeneratedBlockEntityWrite(chunk, write)
    }

    private fun applyGeneratedBlockEntityWrite(
        chunk: Chunk,
        write: ChunkGeneratedBlockEntityWrite,
    ) {
        if (write.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        val localX = write.x and 15
        val localZ = write.z and 15
        if (chunk.getBlockStateId(localX, write.y, localZ) != write.expectedStateId) return
        chunk.setBlockEntity(ChunkBlockEntity(localX, write.y, localZ, write.type, write.data))
    }

    private fun deliverGeneratedBlockTicks(ticks: List<ChunkGeneratedBlockTick>) {
        for (tick in ticks) {
            val targetKey = key(tick.x shr 4, tick.z shr 4)
            val target = chunksByPos[targetKey]
            if (target == null) {
                pendingGeneratedBlockTicks.getOrPut(targetKey, ::mutableListOf) += tick
            } else {
                applyGeneratedBlockTick(target, tick)
            }
        }
    }

    private fun applyPendingGeneratedBlockTicks(chunk: Chunk) {
        val ticks = pendingGeneratedBlockTicks.remove(key(chunk.chunkX, chunk.chunkZ)) ?: return
        for (tick in ticks) applyGeneratedBlockTick(chunk, tick)
    }

    private fun applyGeneratedBlockTick(
        chunk: Chunk,
        tick: ChunkGeneratedBlockTick,
    ) {
        if (tick.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return
        val localX = tick.x and 15
        val localZ = tick.z and 15
        if (chunk.getBlockStateId(localX, tick.y, localZ) != tick.expectedStateId) return
        chunk.scheduleBlockTick(
            ChunkScheduledTick(tick.type, localX, tick.y, localZ, tick.delay, tick.priority),
        )
    }

    /**
     * Who installs the chunks the generation pool produces.
     *
     * The global owner rather than a region's: a chunk that has just been generated belongs to no
     * region yet (the split that decides is worked out from the chunks that *are* loaded), so
     * handing it to one of them meant every install in the world piling onto whichever region was
     * first. With a hundred regions that one region never finished its turn and the barrier waited
     * on it, which read as the server freezing.
     */
    public var installOwner: Owner? = null

    /** Hands [chunk] over to be installed, or installs it here when nobody owns this world. */
    public fun installLater(chunk: Chunk) {
        val target = installOwner ?: owner
        if (target == null) {
            install(chunk)
            return
        }
        target.submit { install(chunk) }
    }

    public fun getBlockState(
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val chunk = loadedChunks.get(key(x shr 4, z shr 4)) ?: return null
        // Above the sky or below the bedrock is not a block, which is what a mob that fell out of
        // the world asks about: the chunk itself refuses the question rather than answering it.
        if (y < chunk.verticalBounds.minY || y > chunk.verticalBounds.maxY) return null
        return chunk.getBlockStateId(x and 15, y, z and 15)
    }

    /** Returns `true` if the position falls inside a loaded chunk (and the block was set), `false` otherwise. */
    public fun setBlockState(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean {
        mine("A block")
        // No lock: the owner is the only writer of blocks, and the chunk it writes into is found in
        // the same lock-free snapshot a read uses. What the lock still guards is the *set* of
        // chunks, which the generation pool adds to from its own threads.
        val chunk = loadedChunks.get(key(x shr 4, z shr 4)) ?: return false
        chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        rememberSculkSensor(x, y, z, stateId)
        return true
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
        mine("A scheduled fluid tick")
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
        mine("A scheduled tick")
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

        /** Set when a merge changed the count, so whoever can see it is told the new stack. */
        public var countChanged: Boolean = false

        public val canBePickedUp: Boolean get() = pickupDelay <= 0
    }

    /** Adds a newly spawned (or re-added, for a partial pickup) dropped item to this world. */
    public fun spawnItem(dropped: DroppedItem): DroppedItem =
        mutex.withLock {
            mine("A dropped item")
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

    /** The live entities standing in this chunk, which is what gets written beside its blocks. */
    public fun entitiesIn(
        chunkX: Int,
        chunkZ: Int,
    ): List<ServerEntity> =
        mutex.withLock {
            entities.values.filter {
                !it.removed && floor(it.x).toInt() shr 4 == chunkX && floor(it.z).toInt() shr 4 == chunkZ
            }
        }

    /** Which chunks hold entities, so a save knows to write them even when no block moved. */
    public fun chunksWithEntities(): Set<Pair<Int, Int>> =
        mutex.withLock {
            entities.values
                .filter { !it.removed }
                .mapTo(mutableSetOf()) { floor(it.x).toInt() shr 4 to (floor(it.z).toInt() shr 4) }
        }

    /** Forgets the entities in this chunk: they are on disk now, and the chunk is going. */
    public fun forgetEntitiesIn(
        chunkX: Int,
        chunkZ: Int,
    ) {
        mutex.withLock {
            entities.values
                .filter { floor(it.x).toInt() shr 4 == chunkX && floor(it.z).toInt() shr 4 == chunkZ }
                .forEach { entities.remove(it.entityId) }
        }
    }

    /**
     * The entities a chunk brought back from disk, waiting for the tick loop to give them ids.
     *
     * They cannot be spawned where the chunk is loaded: that happens on a generation thread, and an
     * entity id belongs to the server. parity: vanilla defers the same work to the entity manager.
     */
    private val pendingEntities = mutableListOf<NbtCompound>()

    public fun takePendingEntities(): List<NbtCompound> =
        mutex.withLock {
            if (pendingEntities.isEmpty()) {
                emptyList()
            } else {
                val taken = pendingEntities.toList()
                pendingEntities.clear()
                taken
            }
        }

    public fun entity(entityId: Int): ServerEntity? = mutex.withLock { entities[entityId] }

    public fun spawnEntity(entity: ServerEntity): ServerEntity =
        mutex.withLock {
            mine("An entity")
            // parity: `finalizeSpawn`, of which the only part this build has is a cube's size: its
            // speed, health and damage all come from it.
            CubeMobs.applySize(entity)
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
            mine("A dropped item")
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
                if (item.age >= LIFETIME_TICKS) {
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
        item: DroppedItem,
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

    /** Two stacks only merge when they carry the same extras, which for now means neither carries any. */
    private fun sameComponents(
        a: NbtCompound,
        b: NbtCompound,
    ): Boolean = a.keys.isEmpty() && b.keys.isEmpty()

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
        private val CAVE_AIR_STATE_ID = Blocks.CAVE_AIR.stateId
        private val VOID_AIR_STATE_ID = Blocks.VOID_AIR.stateId
    }
}
