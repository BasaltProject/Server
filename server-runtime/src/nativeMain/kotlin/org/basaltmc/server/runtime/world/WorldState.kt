package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.sched.Owner
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.NoiseWorldGenerator
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.collections.Long2ObjectMap
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.concurrent.Volatile
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Shared view over the loaded chunks. The owner writes without any lock, using the same
 * lock-free snapshot a read uses; the lock is reserved for the periodic save and the unload
 * sweep, the two threads that are deliberately not the owner. Known hole: saving can encode a
 * chunk while the owner is still writing blocks into it. [loadOrGenerate] must read the region
 * files before generating, or a visited chunk comes back regenerated with the player's edits gone.
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

    // Every collaborator below shares this same mutex rather than owning one of its own: splitting
    // lock granularity would change race outcomes between the tick thread and the save/eviction
    // threads, which is a real behavior change even where it has nothing to do with vanilla parity.
    private val primedTntQueue = PrimedTntQueue(mutex)
    private val pendingDecorations = PendingChunkDecorations(mutex)
    private val sculkSensors = SculkSensorTracker(mutex)
    private val entityRegistry = EntityRegistry(mutex)
    private val itemPhysics = DroppedItemPhysics(this, mutex)

    init {
        // Tests and embedders may seed a world with detached generated chunks. Production starts
        // empty, but those chunks still deserve the same cross-border handoff as [install].
        for (chunk in initialChunks) {
            pendingDecorations.addPendingGeneratedEntities(chunk.takeGeneratedEntitiesMatching { Mobs.isMob(it.type) })
            pendingDecorations.applyPendingGeneratedBlockWrites(chunk)
            pendingDecorations.applyPendingGeneratedBlockEntityWrites(chunk)
            pendingDecorations.applyPendingGeneratedBlockTicks(chunk)
            pendingDecorations.deliverGeneratedBlockWrites(chunk.takeGeneratedBlockWrites(), chunksByPos::get)
            pendingDecorations.deliverGeneratedBlockEntityWrites(
                chunk.takeGeneratedBlockEntityWrites(),
                chunksByPos::get,
            )
            pendingDecorations.deliverGeneratedBlockTicks(chunk.takeGeneratedBlockTicks(), chunksByPos::get)
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
                    pendingDecorations.forgetGeneratedEntities(chunkKey)
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
                pendingDecorations.applyPendingGeneratedBlockWrites(chunk)
                pendingDecorations.applyPendingGeneratedBlockEntityWrites(chunk)
                pendingDecorations.applyPendingGeneratedBlockTicks(chunk)
                pendingDecorations.deliverGeneratedBlockWrites(chunk.takeGeneratedBlockWrites(), chunksByPos::get)
                pendingDecorations.deliverGeneratedBlockEntityWrites(
                    chunk.takeGeneratedBlockEntityWrites(),
                    chunksByPos::get,
                )
                pendingDecorations.deliverGeneratedBlockTicks(chunk.takeGeneratedBlockTicks(), chunksByPos::get)
                loadedChunks = snapshotOf(chunksByPos)
                chunkVersion.addAndGet(1)
                entityRegistry.addPendingEntities(chunk.takeSavedEntities())
                pendingDecorations.addPendingGeneratedEntities(
                    chunk.takeGeneratedEntitiesMatching { Mobs.isMob(it.type) },
                )
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
     * Who installs the chunks the generation pool produces.
     *
     * The global owner rather than a region's: a chunk that has just been generated belongs to no
     * region yet (the split that decides is worked out from the chunks that *are* loaded), so
     * handing it to one of them meant every install in the world piling onto whichever region was
     * first. With a hundred regions that one region never finished its turn and the barrier waited
     * on it, which read as the server freezing.
     */
    public var installOwner: Owner? = null

    /**
     * Told when a chunk arrives, which is where `world.chunk_load` is fired from.
     *
     * A hook rather than the bus itself: this module is below the plugin one, and a world knowing
     * what a plugin is would be a flèche pointing the wrong way.
     */
    public var onChunkInstalled: ((Chunk) -> Unit)? = null

    /** Hands [chunk] over to be installed, or installs it here when nobody owns this world. */
    public fun installLater(chunk: Chunk) {
        val target = installOwner ?: owner
        if (target == null) {
            install(chunk)
            onChunkInstalled?.invoke(chunk)
            return
        }
        target.submit {
            install(chunk)
            onChunkInstalled?.invoke(chunk)
        }
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
        val changed = chunk.setBlockStateId(x and 15, y, z and 15, stateId)
        if (changed) {
            // A skylight opening or a torch reaches across chunk borders. Cached whole-chunk light
            // is cheaper than incremental light here, but every cached neighbour that could have
            // read the old block must be discarded together.
            for (offsetX in -1..1) {
                for (offsetZ in -1..1) {
                    loadedChunks.get(key((x shr 4) + offsetX, (z shr 4) + offsetZ))?.invalidateLight()
                }
            }
        }
        sculkSensors.remember(x, y, z, stateId)
        return true
    }

    /** Whether any known sensor is within [range] blocks of this point. */
    public fun hasSculkSensorNear(
        x: Double,
        y: Double,
        z: Double,
        range: Int,
    ): Boolean = sculkSensors.hasNear(x, y, z, range)

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
     * Bounded because this runs once per player per tick holding a lock, against the lock the
     * generation workers need. Walked in coordinate order rather than sorted, so an entity's id
     * still depends only on position; [evictClean] is what forgets them, when the chunk goes.
     */
    public fun generatedEntities(
        allocateEntityId: () -> Int,
        centerChunkX: Int,
        centerChunkZ: Int,
        radiusChunks: Int,
    ): List<GeneratedWorldEntity> =
        pendingDecorations.generatedEntities(
            allocateEntityId,
            centerChunkX,
            centerChunkZ,
            radiusChunks,
            chunksByPos::get,
        )

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
    public fun spawnItem(dropped: DroppedItem): DroppedItem {
        mine("A dropped item")
        return itemPhysics.spawnItem(dropped)
    }

    /** Every dropped item in this world, as a snapshot safe to iterate outside the lock. */
    public fun items(): List<DroppedItem> = itemPhysics.items()

    // --- Summoned entities ---

    /** Everything `/summon` put here, alive or just removed and not yet forgotten by clients. */
    public fun entities(): List<ServerEntity> = entityRegistry.entities()

    internal fun hasNaturalSpawnEntityObstruction(
        spawnBox: Aabb,
        entityBox: Aabb,
    ): Boolean = entityRegistry.hasNaturalSpawnObstruction(spawnBox, entityBox)

    /** The live entities standing in this chunk, which is what gets written beside its blocks. */
    public fun entitiesIn(
        chunkX: Int,
        chunkZ: Int,
    ): List<ServerEntity> = entityRegistry.entitiesIn(chunkX, chunkZ)

    /** Which chunks hold entities, so a save knows to write them even when no block moved. */
    public fun chunksWithEntities(): Set<Pair<Int, Int>> = entityRegistry.chunksWithEntities()

    /** Forgets the entities in this chunk: they are on disk now, and the chunk is going. */
    public fun forgetEntitiesIn(
        chunkX: Int,
        chunkZ: Int,
    ) {
        entityRegistry.forgetEntitiesIn(chunkX, chunkZ)
    }

    /**
     * The entities a chunk brought back from disk, waiting for the tick loop to give them ids.
     *
     * They cannot be spawned where the chunk is loaded: that happens on a generation thread, and an
     * entity id belongs to the server. parity: vanilla defers the same work to the entity manager.
     */
    public fun takePendingEntities(): List<NbtCompound> = entityRegistry.takePendingEntities()

    /**
     * Mobs emitted by structure templates, waiting for the server tick to allocate runtime ids.
     * Taking them removes the only pending copy; their chunk descriptor was removed at install so
     * the live entity saved with the chunk cannot be duplicated on the next load.
     */
    public fun takePendingGeneratedEntities(): List<ChunkGeneratedEntity> =
        pendingDecorations.takePendingGeneratedEntities()

    public fun entity(entityId: Int): ServerEntity? = entityRegistry.entity(entityId)

    public fun spawnEntity(entity: ServerEntity): ServerEntity {
        mine("An entity")
        return entityRegistry.spawnEntity(entity)
    }

    /** parity: `TntBlock.explode`, which spawns the entity where the block stood. */
    public fun primeTnt(
        x: Int,
        y: Int,
        z: Int,
    ) {
        primedTntQueue.primeTnt(x, y, z)
    }

    /** Takes what was lit, leaving the queue empty. */
    public fun takePrimedTnt(): List<IntArray> = primedTntQueue.takePrimedTnt()

    /** Drops everything marked removed, and returns their ids so clients can be told once. */
    public fun collectRemovedEntities(): List<Int> = entityRegistry.collectRemovedEntities()

    /** A snapshot of every dropped item within [radius] blocks of a point, nearest first. */
    public fun nearbyItems(
        x: Double,
        y: Double,
        z: Double,
        radius: Double,
    ): List<DroppedItem> = itemPhysics.nearbyItems(x, y, z, radius)

    /** Removes and returns the dropped item with this entity id, or `null` if it is already gone (picked up, despawned). */
    public fun removeItem(entityId: Int): DroppedItem? {
        mine("A dropped item")
        return itemPhysics.removeItem(entityId)
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
    public fun tickItems(region: Region? = null): List<Int> = itemPhysics.tickItems(region)

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
    }
}
