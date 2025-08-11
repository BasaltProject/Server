package org.basaltmc.server.runtime

import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkDataCodec
import org.basaltmc.server.world.ChunkPos
import org.basaltmc.server.world.ChunkTracker
import org.basaltmc.server.world.light.LightWindow
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchStartPacket
import org.basaltmc.wire.protocol.play.ChunkBiomeData
import org.basaltmc.wire.protocol.play.ChunkBlockEntityInfo
import org.basaltmc.wire.protocol.play.ChunksBiomesPacket
import org.basaltmc.wire.protocol.play.ForgetLevelChunkPacket
import org.basaltmc.wire.protocol.play.GameEventPacket
import org.basaltmc.wire.protocol.play.Heightmap
import org.basaltmc.wire.protocol.play.HeightmapType
import org.basaltmc.wire.protocol.play.MapChunkPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheCenterPacket
import org.basaltmc.wire.protocol.play.SetChunkCacheRadiusPacket
import org.basaltmc.wire.protocol.play.SetSimulationDistancePacket
import org.basaltmc.wire.protocol.play.UpdateLightPacket
import kotlin.math.abs

/**
 * The chunk-streaming state and its packets: view radius, [ChunkTracker], the queue of chunks
 * generated but not yet on the wire, and the client's own reported chunks-per-tick budget.
 *
 * [enterPlay][ClientSession], [respawn][ClientSession] and [switchWorld][ClientSession] each reset
 * this differently enough (waiting for the ground vs. re-centering vs. dropping the old world's
 * queue) that each gets its own method here rather than one shared "reset".
 */
internal class ChunkStreamer(
    private val owner: ClientSession,
) {
    private val chunkTracker = ChunkTracker()
    private var viewRadius = 0
    private var centerChunkX = 0
    private var centerChunkZ = 0

    // Requested from the generation queue, not yet on the wire. Both this connection's thread and
    // the tick loop drain it, so it is guarded.
    private val chunkStreamMutex = NativeMutex()
    private val pendingChunks = mutableListOf<ChunkPos>()

    // Until the client has told us: enough to fill a view distance in a few seconds, far below the
    // rate the server can produce at. Guessing high is what floods a client that then stops
    // draining its socket at all.
    internal var desiredChunksPerTick: Float = ClientSession.INITIAL_CHUNKS_PER_TICK
    internal var sendBudget: Float = 0f

    private val loggedUnknownBlockEntities = mutableSetOf<Identifier>()

    /**
     * The chunk-radius half of a fresh join. parity: `PlayerList.placeNewPlayer`'s own chunk-radius
     * packets, sent right after the login packet.
     *
     * Seeds the tracker with exactly the square about to be sent by [sendInitialChunks], so later
     * crossings compute deltas relative to it instead of re-sending everything as "newly loaded" on
     * the first move.
     */
    internal fun initialJoin(
        x: Double,
        z: Double,
        radius: Int,
    ): ChunkTracker.Delta {
        centerChunkX = x.toInt() shr 4
        centerChunkZ = z.toInt() shr 4
        viewRadius = radius
        val initialChunks = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        owner.sendCompressed(SetChunkCacheCenterPacket.ID) {
            SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it)
        }
        owner.sendCompressed(SetChunkCacheRadiusPacket.ID) { SetChunkCacheRadiusPacket(radius).write(it) }
        owner.sendCompressed(SetSimulationDistancePacket.ID) { SetSimulationDistancePacket(radius).write(it) }
        return initialChunks
    }

    /**
     * The rest of a fresh join. `level_chunks_load_start` must go out first or the loading screen
     * never dismisses. Only the ground the player lands on is waited for, one ring wider so every
     * chunk sent already has its eight neighbours; the rest streams in through [flushReadyChunks].
     */
    internal fun sendInitialChunks(initialChunks: ChunkTracker.Delta) {
        owner.sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        val awaited =
            (-(ClientSession.LOGIN_CHUNK_RADIUS + 1)..(ClientSession.LOGIN_CHUNK_RADIUS + 1))
                .flatMap { dx ->
                    (-(ClientSession.LOGIN_CHUNK_RADIUS + 1)..(ClientSession.LOGIN_CHUNK_RADIUS + 1)).map { dz ->
                        (centerChunkX + dx) to (centerChunkZ + dz)
                    }
                }.sortedBy { squaredDistanceToCenter(it.first, it.second) }
        owner.generationQueue.awaitAll(awaited)
        val loginChunks =
            awaited
                .filter { position ->
                    abs(position.first - centerChunkX) <= ClientSession.LOGIN_CHUNK_RADIUS &&
                        abs(position.second - centerChunkZ) <= ClientSession.LOGIN_CHUNK_RADIUS
                }.mapNotNull { position -> owner.world.chunkIfLoaded(position.first, position.second) }
        owner.sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in loginChunks) sendChunk(chunk)
        owner.sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(loginChunks.size).write(it) }

        // Everything else the view distance covers, queued for the outbound loop to stream.
        val sent = loginChunks.map { chunk -> chunk.chunkX to chunk.chunkZ }.toSet()
        val streaming = initialChunks.toLoad.filter { (it.x to it.z) !in sent }
        chunkStreamMutex.withLock { for (pos in streaming) pendingChunks.add(pos) }
        for (pos in streaming) owner.generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
    }

    /**
     * parity: `PlayerList.respawn`'s chunk half. The client throws its whole level away when it
     * handles the respawn packet, so [chunkTracker] is reset to match, otherwise it would believe
     * the client still had the chunks around the spawn point and send nothing back.
     *
     * No radius packet here, unlike [initialJoin] and [resetForWorldSwitch]: the radius has not
     * changed, only the center has.
     */
    internal fun resetForRespawn(
        spawnX: Double,
        spawnZ: Double,
    ) {
        centerChunkX = spawnX.toInt() shr 4
        centerChunkZ = spawnZ.toInt() shr 4
        chunkTracker.reset()
        val delta = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        owner.sendCompressed(SetChunkCacheCenterPacket.ID) {
            SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it)
        }
        owner.sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        chunkStreamMutex.withLock { for (pos in delta.toLoad) pendingChunks.add(pos) }
        for (pos in delta.toLoad) owner.generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
    }

    /**
     * parity of intent with a respawn's chunk reset, for a world change instead of death: the
     * client throws its level away on this `Respawn` too. Radius is included since the destination
     * world may configure a different view distance, and anything still queued from the old world
     * is dropped rather than streamed into the new one.
     */
    internal fun resetForWorldSwitch(
        x: Double,
        z: Double,
        radius: Int,
    ) {
        centerChunkX = x.toInt() shr 4
        centerChunkZ = z.toInt() shr 4
        viewRadius = radius
        chunkTracker.reset()
        val delta = chunkTracker.update(centerChunkX, centerChunkZ, viewRadius)
        owner.sendCompressed(SetChunkCacheCenterPacket.ID) {
            SetChunkCacheCenterPacket(centerChunkX, centerChunkZ).write(it)
        }
        owner.sendCompressed(SetChunkCacheRadiusPacket.ID) { SetChunkCacheRadiusPacket(viewRadius).write(it) }
        owner.sendCompressed(GameEventPacket.ID) {
            GameEventPacket(GameEventPacket.LEVEL_CHUNKS_LOAD_START, 0f).write(it)
        }
        chunkStreamMutex.withLock {
            pendingChunks.clear()
            for (pos in delta.toLoad) pendingChunks.add(pos)
        }
        for (pos in delta.toLoad) owner.generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
    }

    /**
     * Sends one chunk again, for a change too large to express as block updates.
     *
     * `/place template` writes a whole structure at once: hundreds of blocks across several chunks,
     * where one packet per chunk is both smaller and simpler than the section updates would be.
     */
    internal fun resendChunk(
        chunkX: Int,
        chunkZ: Int,
    ) {
        val chunk = owner.currentWorld?.state?.chunkIfLoaded(chunkX, chunkZ) ?: return
        owner.sendPlayPacket(ChunkBatchStartPacket.ID) { }
        sendChunk(chunk)
        owner.sendPlayPacket(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(1).write(it) }
    }

    /**
     * New biomes for columns this client already has, without resending them.
     *
     * parity: `ClientboundChunksBiomesPacket`, which vanilla sends from `/fillbiome` for exactly
     * this reason: a chunk resend would drop and rebuild the column, and the biome is the only
     * thing that changed.
     */
    internal fun sendBiomes(chunks: List<Pair<Int, Int>>) {
        val world = owner.currentWorld ?: return
        val columns =
            chunks.mapNotNull { (chunkX, chunkZ) ->
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: return@mapNotNull null
                ChunkBiomeData(chunkX, chunkZ, ChunkDataCodec.encodeBiomes(chunk))
            }
        if (columns.isEmpty()) return
        owner.sendCompressed(ChunksBiomesPacket.ID) { ChunksBiomesPacket(columns).write(it) }
    }

    private fun sendChunk(chunk: Chunk) {
        val chunkData = ChunkDataCodec.encodeSections(chunk)
        val world = owner.currentWorld
        val light =
            ChunkDataCodec.encodeLight(
                chunk,
                world?.declaration?.dimension?.hasSkylight ?: true,
                // The chunk and the eight around it, straight off the chunk objects: light reaches
                // less than a chunk, so that is everything that can shine into this one.
                LightWindow(chunk) { chunkX, chunkZ -> world?.state?.chunkIfLoaded(chunkX, chunkZ) },
            )
        val heightmap = ChunkDataCodec.encodeHeightmap(chunk)
        owner.sendCompressed(MapChunkPacket.ID) {
            MapChunkPacket(
                x = chunk.chunkX,
                z = chunk.chunkZ,
                heightmaps = listOf(Heightmap(HeightmapType.MOTION_BLOCKING, heightmap)),
                chunkData = chunkData,
                blockEntities =
                    chunk.blockEntities().mapNotNull { blockEntity ->
                        val typeId =
                            BlockEntityTypes.idOf(blockEntity.type)
                                ?: return@mapNotNull unknownBlockEntity(blockEntity.type)
                        ChunkBlockEntityInfo(
                            packedXZ = (blockEntity.localX shl 4) or blockEntity.localZ,
                            y = blockEntity.y,
                            typeId = typeId,
                            tag = blockEntity.data,
                        )
                    },
                skyLightMask = light.skyLightMask,
                blockLightMask = light.blockLightMask,
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = light.emptyBlockLightMask,
                skyLight = light.skyLight,
                blockLight = light.blockLight,
            ).write(it)
        }
        owner.sendCompressed(UpdateLightPacket.ID) {
            UpdateLightPacket(
                chunkX = chunk.chunkX,
                chunkZ = chunk.chunkZ,
                skyLightMask = light.skyLightMask,
                blockLightMask = light.blockLightMask,
                emptySkyLightMask = light.emptySkyLightMask,
                emptyBlockLightMask = light.emptyBlockLightMask,
                skyLight = light.skyLight,
                blockLight = light.blockLight,
            ).write(it)
        }
    }

    /**
     * Leaves one block entity out of the chunk rather than failing the packet. It used to throw,
     * which dropped the player: flying into a village, whose banners this build had no ID for, was
     * enough. The block itself still arrives, only the data the client would decorate it with does
     * not, and the log names the type once per session so the gap is visible.
     */
    private fun unknownBlockEntity(type: Identifier): ChunkBlockEntityInfo? {
        if (loggedUnknownBlockEntities.add(type)) {
            owner.logger.warn { "No protocol ID for block entity type $type; leaving it out of the chunk." }
        }
        return null
    }

    /**
     * Re-centers [chunkTracker] on the player's current chunk and applies the resulting delta: newly
     * in-range chunks are generated on demand ([owner.generationQueue][ClientSession], across a
     * worker pool, not serially) and sent, chunks that fell out of range are forgotten client-side. A
     * no-op when the player hasn't crossed into a new chunk, so this stays cheap on every movement
     * packet, not just the ones that matter.
     */
    internal fun streamChunksAround(
        x: Double,
        z: Double,
    ) {
        val chunkX = x.toInt() shr 4
        val chunkZ = z.toInt() shr 4
        if (chunkX == centerChunkX && chunkZ == centerChunkZ) return
        val previousCenterX = centerChunkX
        val previousCenterZ = centerChunkZ
        centerChunkX = chunkX
        centerChunkZ = chunkZ

        val delta = chunkTracker.update(chunkX, chunkZ, viewRadius)
        if (delta.toLoad.isEmpty() && delta.toUnload.isEmpty()) return

        owner.sendCompressed(SetChunkCacheCenterPacket.ID) { SetChunkCacheCenterPacket(chunkX, chunkZ).write(it) }
        chunkStreamMutex.withLock {
            for (pos in delta.toLoad) pendingChunks.add(pos)
            for (pos in delta.toUnload) pendingChunks.remove(pos)
        }
        for (pos in delta.toLoad) {
            owner.generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
        }
        prefetchAhead(chunkX, chunkZ, movedX = chunkX - previousCenterX, movedZ = chunkZ - previousCenterZ)
        // Unloads go out straight away rather than waiting behind generation: the client can drop
        // what it no longer needs before the replacements are even built.
        for (pos in delta.toUnload) {
            owner.sendCompressed(ForgetLevelChunkPacket.ID) { ForgetLevelChunkPacket(pos.x, pos.z).write(it) }
        }
        // No flush here: the outbound loop is the only drainer, so the client's requested rate is
        // honored, and the read thread never stalls on a burst of chunk encoding and zlib.
    }

    /**
     * Asks for a margin of chunks just beyond the view distance, in whichever direction the player
     * is actually travelling. Without this the first time a chunk is wanted is the moment it comes
     * into view, so at speed the player outruns generation no matter how fast a single chunk is;
     * with it, the chunks ahead are already warm by the time the view reaches them. They are never
     * sent from here, only generated: they go out normally once [chunkTracker] takes them in.
     *
     * Requested behind everything already visible ([ClientSession.PREFETCH_PRIORITY_PENALTY]), so
     * speculative work can never delay a chunk the player is currently staring at.
     */
    private fun prefetchAhead(
        chunkX: Int,
        chunkZ: Int,
        movedX: Int,
        movedZ: Int,
    ) {
        if (movedX == 0 && movedZ == 0) return
        val stepX = movedX.coerceIn(-1, 1)
        val stepZ = movedZ.coerceIn(-1, 1)
        for (margin in 1..ClientSession.PREFETCH_MARGIN_CHUNKS) {
            val aheadX = chunkX + stepX * (viewRadius + margin)
            val aheadZ = chunkZ + stepZ * (viewRadius + margin)
            for (offset in -viewRadius..viewRadius) {
                val x = if (stepX != 0) aheadX else chunkX + offset
                val z = if (stepZ != 0) aheadZ else chunkZ + offset
                owner.generationQueue.request(
                    x,
                    z,
                    squaredDistanceToCenter(x, z) + ClientSession.PREFETCH_PRIORITY_PENALTY,
                )
            }
        }
    }

    /**
     * Sends whichever requested chunks have finished generating, nearest first, leaving the rest
     * pending and never blocking on generation.
     *
     * Driven from this connection's outbound loop and nowhere else. It used to run from the read
     * loop too, on every movement packet: that bypassed the client's own requested chunks-per-tick
     * budget and parked the read thread behind bursts of chunk encoding and zlib. Capped per call
     * so one flush cannot monopolize the socket.
     */
    internal fun flushReadyChunks(limit: Int = ClientSession.MAX_BURST_CHUNKS.toInt()): Int {
        if (owner.state != ProtocolState.PLAY) return 0
        val ready =
            chunkStreamMutex.withLock {
                if (pendingChunks.isEmpty()) return 0
                pendingChunks.sortBy { squaredDistanceToCenter(it.x, it.z) }
                val found = mutableListOf<Chunk>()
                val iterator = pendingChunks.iterator()
                while (iterator.hasNext() && found.size < limit) {
                    val pos = iterator.next()
                    val chunk = owner.world.chunkIfLoaded(pos.x, pos.z)
                    if (chunk == null) {
                        // Requested, then unloaded again before it could be sent. Ask for it once
                        // more rather than waiting forever on a chunk nothing is going to produce:
                        // the tracker already believes the client has it, so nothing else would.
                        owner.generationQueue.request(pos.x, pos.z, squaredDistanceToCenter(pos.x, pos.z))
                        continue
                    }
                    // parity: vanilla only sends a chunk once it is FULL, which it cannot be until
                    // the eight around it exist: light worked out with a neighbour missing lets the
                    // sky in from nowhere, and the client keeps whatever it is sent. Under water
                    // that is the difference between a dim sea floor and one lit like open air.
                    if (!neighboursLoaded(pos.x, pos.z)) continue
                    found.add(chunk)
                    iterator.remove()
                }
                found
            }
        if (ready.isEmpty()) return 0
        owner.sendCompressed(ChunkBatchStartPacket.ID) { }
        for (chunk in ready) sendChunk(chunk)
        owner.sendCompressed(ChunkBatchFinishedPacket.ID) { ChunkBatchFinishedPacket(ready.size).write(it) }
        return ready.size
    }

    /** Whether the eight chunks around this one are in memory, asking for the ones that are not. */
    private fun neighboursLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean {
        var complete = true
        for (offsetZ in -1..1) {
            for (offsetX in -1..1) {
                if (offsetX == 0 && offsetZ == 0) continue
                val x = chunkX + offsetX
                val z = chunkZ + offsetZ
                if (owner.world.isLoaded(x, z)) continue
                complete = false
                owner.generationQueue.request(x, z, squaredDistanceToCenter(x, z))
            }
        }
        return complete
    }

    private fun squaredDistanceToCenter(
        chunkX: Int,
        chunkZ: Int,
    ): Int {
        val dx = chunkX - centerChunkX
        val dz = chunkZ - centerChunkZ
        return dx * dx + dz * dz
    }
}
