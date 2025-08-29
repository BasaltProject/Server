package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.CubeMobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.floor

/**
 * Everything `/summon` (or a loaded chunk, or a plugin) put into this world: the live [ServerEntity]
 * table, plus the two staging lists a chunk load feeds before the tick loop can allocate runtime
 * ids for what it brought back.
 *
 * Shares [WorldState]'s single [mutex] rather than owning its own, for the same reason every other
 * collaborator does: splitting lock granularity changes race outcomes between the tick thread and
 * the save/eviction threads.
 */
internal class EntityRegistry(
    private val mutex: NativeMutex,
) {
    private val entities = LinkedHashMap<Int, ServerEntity>()

    /**
     * The entities a chunk brought back from disk, waiting for the tick loop to give them ids.
     *
     * They cannot be spawned where the chunk is loaded: that happens on a generation thread, and an
     * entity id belongs to the server. parity: vanilla defers the same work to the entity manager.
     */
    private val pendingEntities = mutableListOf<NbtCompound>()

    /** Everything `/summon` put here, alive or just removed and not yet forgotten by clients. */
    public fun entities(): List<ServerEntity> = mutex.withLock { entities.values.toList() }

    /** Hot-path obstruction query without copying the whole entity registry for every spawn attempt. */
    public fun hasNaturalSpawnObstruction(
        spawnBox: Aabb,
        entityBox: Aabb,
    ): Boolean =
        mutex.withLock {
            for (entity in entities.values) {
                if (NaturalSpawnObstruction.entityBlocksSpawn(entity, spawnBox, entityBox)) return@withLock true
            }
            false
        }

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

    /** Queues a chunk's saved entities. Called from [WorldState.install], already under the lock. */
    public fun addPendingEntities(saved: List<NbtCompound>) {
        pendingEntities += saved
    }

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
            // A cube's speed, health and damage all derive from the size chosen by finalizeSpawn.
            CubeMobs.applySize(entity)
            entities[entity.entityId] = entity
            entity
        }

    /** Drops everything marked removed, and returns their ids so clients can be told once. */
    public fun collectRemovedEntities(): List<Int> =
        mutex.withLock {
            val gone = entities.values.filter { it.removed }.map { it.entityId }
            for (id in gone) entities.remove(id)
            gone
        }
}
