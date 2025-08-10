package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedBlockEntityWrite
import org.basaltmc.server.world.ChunkGeneratedBlockTick
import org.basaltmc.server.world.ChunkGeneratedBlockWrite
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.wire.io.NativeMutex
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** One generated entity's runtime identity, handed out the first time a client asks about it. */
@OptIn(ExperimentalUuidApi::class)
public data class GeneratedWorldEntity(
    val entityId: Int,
    val uuid: Uuid,
    val descriptor: ChunkGeneratedEntity,
)

/**
 * Decorations that crossed a chunk border during generation (block writes, block entities,
 * scheduled ticks) and the mobs a template spawned, plus the runtime identities handed out for
 * the generated entities a chunk carries.
 *
 * The deliver/apply methods take no lock of their own: callers already hold [WorldState]'s
 * [mutex]. [generatedEntities] and [takePendingGeneratedEntities] are reached without one already
 * held, so they take [mutex] themselves.
 */
@OptIn(ExperimentalUuidApi::class)
internal class PendingChunkDecorations(
    private val mutex: NativeMutex,
) {
    private val generatedEntitiesByKey = mutableMapOf<GeneratedEntityKey, GeneratedWorldEntity>()
    private val pendingGeneratedEntities = mutableListOf<ChunkGeneratedEntity>()
    private val pendingGeneratedBlockWrites = HashMap<Long, MutableList<ChunkGeneratedBlockWrite>>()
    private val pendingGeneratedBlockEntityWrites = HashMap<Long, MutableList<ChunkGeneratedBlockEntityWrite>>()
    private val pendingGeneratedBlockTicks = HashMap<Long, MutableList<ChunkGeneratedBlockTick>>()

    /** Queues the mobs a just-installed chunk's structures brought with it. Called with no lock held. */
    public fun addPendingGeneratedEntities(entities: List<ChunkGeneratedEntity>) {
        pendingGeneratedEntities += entities
    }

    /** Forgets the generated-entity identities that belonged to this now-evicted chunk. */
    public fun forgetGeneratedEntities(chunkKey: Long) {
        generatedEntitiesByKey.keys.removeAll { it.chunkKey == chunkKey }
    }

    /**
     * Delivers decorations that crossed out of their source chunk without ever wrapping them back
     * into it. A target that has not arrived yet keeps the write until its own install.
     */
    public fun deliverGeneratedBlockWrites(
        writes: List<ChunkGeneratedBlockWrite>,
        chunkAt: (Long) -> Chunk?,
    ) {
        for (write in writes) {
            val targetKey = key(write.x shr 4, write.z shr 4)
            val target = chunkAt(targetKey)
            if (target == null) {
                pendingGeneratedBlockWrites.getOrPut(targetKey, ::mutableListOf) += write
            } else {
                applyGeneratedBlockWrite(target, write)
            }
        }
    }

    public fun applyPendingGeneratedBlockWrites(chunk: Chunk) {
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
        if (write.replaceAnyState || current == write.expectedStateId) {
            chunk.setBlockStateId(localX, write.y, localZ, write.stateId)
        }
    }

    public fun deliverGeneratedBlockEntityWrites(
        writes: List<ChunkGeneratedBlockEntityWrite>,
        chunkAt: (Long) -> Chunk?,
    ) {
        for (write in writes) {
            val targetKey = key(write.x shr 4, write.z shr 4)
            val target = chunkAt(targetKey)
            if (target == null) {
                pendingGeneratedBlockEntityWrites.getOrPut(targetKey, ::mutableListOf) += write
            } else {
                applyGeneratedBlockEntityWrite(target, write)
            }
        }
    }

    public fun applyPendingGeneratedBlockEntityWrites(chunk: Chunk) {
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

    public fun deliverGeneratedBlockTicks(
        ticks: List<ChunkGeneratedBlockTick>,
        chunkAt: (Long) -> Chunk?,
    ) {
        for (tick in ticks) {
            val targetKey = key(tick.x shr 4, tick.z shr 4)
            val target = chunkAt(targetKey)
            if (target == null) {
                pendingGeneratedBlockTicks.getOrPut(targetKey, ::mutableListOf) += tick
            } else {
                applyGeneratedBlockTick(target, tick)
            }
        }
    }

    public fun applyPendingGeneratedBlockTicks(chunk: Chunk) {
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
     * Runtime identities for the entities of generated chunks within [radiusChunks] of
     * ([centerChunkX], [centerChunkZ]).
     *
     * Bounded because this runs once per player per tick holding [mutex], against the lock the
     * generation workers need. Walked in coordinate order rather than sorted, so an entity's id
     * still depends only on position; a chunk eviction is what forgets them, via
     * [forgetGeneratedEntities].
     */
    public fun generatedEntities(
        allocateEntityId: () -> Int,
        centerChunkX: Int,
        centerChunkZ: Int,
        radiusChunks: Int,
        chunkAt: (Long) -> Chunk?,
    ): List<GeneratedWorldEntity> =
        mutex.withLock {
            val result = mutableListOf<GeneratedWorldEntity>()
            for (chunkX in centerChunkX - radiusChunks..centerChunkX + radiusChunks) {
                for (chunkZ in centerChunkZ - radiusChunks..centerChunkZ + radiusChunks) {
                    val chunkKey = key(chunkX, chunkZ)
                    val chunk = chunkAt(chunkKey) ?: continue
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

    private data class GeneratedEntityKey(
        val chunkKey: Long,
        val index: Int,
    )

    /**
     * Mobs emitted by structure templates, waiting for the server tick to allocate runtime ids.
     * Taking them removes the only pending copy; their chunk descriptor was removed at install so
     * the live entity saved with the chunk cannot be duplicated on the next load.
     */
    public fun takePendingGeneratedEntities(): List<ChunkGeneratedEntity> =
        mutex.withLock {
            if (pendingGeneratedEntities.isEmpty()) {
                emptyList()
            } else {
                val taken = pendingGeneratedEntities.toList()
                pendingGeneratedEntities.clear()
                taken
            }
        }

    private fun key(
        chunkX: Int,
        chunkZ: Int,
    ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)
}
