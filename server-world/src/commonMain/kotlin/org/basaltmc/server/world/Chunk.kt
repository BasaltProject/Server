package org.basaltmc.server.world

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/** A generated decoration write whose target lies outside the chunk that produced it. */
public data class ChunkGeneratedBlockWrite(
    public val x: Int,
    public val y: Int,
    public val z: Int,
    public val expectedStateId: Int,
    public val stateId: Int,
)

/** A generated block entity whose target lies outside the chunk that produced it. */
public data class ChunkGeneratedBlockEntityWrite(
    public val x: Int,
    public val y: Int,
    public val z: Int,
    public val expectedStateId: Int,
    public val type: Identifier,
    public val data: NbtCompound,
)

/** A generated scheduled block tick whose target lies outside its source chunk. */
public data class ChunkGeneratedBlockTick(
    public val x: Int,
    public val y: Int,
    public val z: Int,
    public val expectedStateId: Int,
    public val type: Identifier,
    public val delay: Int,
    public val priority: Int = 0,
)

/** A 16-wide column of blocks at ([chunkX], [chunkZ]) in chunk coordinates. */
public class Chunk(
    public val chunkX: Int,
    public val chunkZ: Int,
    public val verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
) {
    private val sections = Array(verticalBounds.sectionCount) { ChunkSection() }

    // parity: Heightmap, maintained as blocks are written rather than rescanned on demand. Scanning
    // cost 2.6 million block reads per generated chunk, decoration alone asking for 10000 columns.
    private val heights = Array(HEIGHTMAP_TYPES.size) { IntArray(16 * 16) { verticalBounds.minY } }
    private var heightsValid = true

    private val blockEntitiesByPosition = LinkedHashMap<Int, ChunkBlockEntity>()
    private val generatedEntitiesBacking = mutableListOf<ChunkGeneratedEntity>()
    private var savedEntitiesBacking: List<NbtCompound> = emptyList()
    private val scheduledBlockTicksBacking = mutableListOf<ChunkScheduledTick>()
    private val scheduledFluidTicksBacking = mutableListOf<ChunkScheduledTick>()
    private val generatedExternalBlockWrites = LinkedHashMap<GeneratedWorldPos, ChunkGeneratedBlockWrite>()
    private val generatedExternalBlockEntityWrites =
        LinkedHashMap<GeneratedWorldPos, ChunkGeneratedBlockEntityWrite>()
    private val generatedExternalBlockTicks = LinkedHashMap<GeneratedWorldPos, ChunkGeneratedBlockTick>()
    private var blockWriteObserver: ((Int, Int, Int, Int, Int) -> Unit)? = null

    public val sectionCount: Int get() = sections.size

    public fun section(index: Int): ChunkSection = sections[index]

    public fun getBlockStateId(
        x: Int,
        y: Int,
        z: Int,
    ): Int = sectionFor(y).getBlockStateId(x, y and 15, z)

    /**
     * Whether this chunk holds anything the region files do not: set by every real block change and
     * cleared by [markClean] once it has been written out. Saving only what changed is what keeps
     * `stop` from reserializing every chunk a session ever loaded.
     */
    public var isDirty: Boolean = false
        private set

    /** Marks a change the block states do not carry, like a furnace counting down inside its own NBT. */
    public fun markDirty() {
        isDirty = true
    }

    /** Called after this chunk has been persisted, or right after loading one straight off disk. */
    public fun markClean() {
        isDirty = false
    }

    public fun setBlockStateId(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        val section = sectionFor(y)
        val observer = blockWriteObserver
        if (observer == null) {
            if (blockEntitiesByPosition.isNotEmpty() &&
                changesBlock(section.getBlockStateId(x, y and 15, z), stateId)
            ) {
                blockEntitiesByPosition.remove(blockEntityKey(x, y, z))
            }
            if (section.setBlockStateId(x, y and 15, z, stateId)) {
                isDirty = true
                updateHeights(x, y, z, stateId)
            }
        } else {
            val oldStateId = section.getBlockStateId(x, y and 15, z)
            section.setBlockStateId(x, y and 15, z, stateId)
            if (oldStateId != stateId) {
                isDirty = true
                updateHeights(x, y, z, stateId)
                if (changesBlock(oldStateId, stateId)) blockEntitiesByPosition.remove(blockEntityKey(x, y, z))
            }
            observer(x, y, z, oldStateId, stateId)
        }
    }

    /**
     * Reads a world-coordinate position while a decoration is being generated. Positions outside
     * this chunk see earlier writes from the same feature instead of wrapping through `and 15`.
     * Unknown neighbour terrain is air; [ChunkGeneratedBlockWrite.expectedStateId] prevents the
     * deferred write from replacing a real non-air block when its target chunk is installed.
     */
    public fun getDecorationBlockStateId(
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int {
        if (y !in verticalBounds.minY..verticalBounds.maxY) return Blocks.AIR.stateId
        if (worldX shr 4 == chunkX && worldZ shr 4 == chunkZ) {
            return getBlockStateId(worldX and 15, y, worldZ and 15)
        }
        return generatedExternalBlockWrites[GeneratedWorldPos(worldX, y, worldZ)]?.stateId ?: Blocks.AIR.stateId
    }

    /**
     * Writes a world-coordinate decoration position. Local writes land immediately; external ones
     * retain their true coordinates until the runtime installs their target chunk. Most features
     * expect unknown external air; water-replacing features can pass their observed expectation.
     */
    public fun setDecorationBlockStateId(
        worldX: Int,
        y: Int,
        worldZ: Int,
        stateId: Int,
        expectedExternalStateId: Int = Blocks.AIR.stateId,
    ) {
        if (y !in verticalBounds.minY..verticalBounds.maxY) return
        if (worldX shr 4 == chunkX && worldZ shr 4 == chunkZ) {
            setBlockStateId(worldX and 15, y, worldZ and 15, stateId)
            return
        }
        val position = GeneratedWorldPos(worldX, y, worldZ)
        val previous = generatedExternalBlockWrites[position]
        val oldStateId = previous?.stateId ?: Blocks.AIR.stateId
        if (oldStateId == stateId) {
            blockWriteObserver?.invoke(worldX - chunkX * 16, y, worldZ - chunkZ * 16, oldStateId, stateId)
            return
        }
        generatedExternalBlockWrites[position] =
            ChunkGeneratedBlockWrite(
                worldX,
                y,
                worldZ,
                previous?.expectedStateId ?: expectedExternalStateId,
                stateId,
            )
        blockWriteObserver?.invoke(worldX - chunkX * 16, y, worldZ - chunkZ * 16, oldStateId, stateId)
    }

    /** Hands deferred cross-chunk decoration writes to the runtime exactly once. */
    public fun takeGeneratedBlockWrites(): List<ChunkGeneratedBlockWrite> {
        if (generatedExternalBlockWrites.isEmpty()) return emptyList()
        val writes = generatedExternalBlockWrites.values.toList()
        generatedExternalBlockWrites.clear()
        return writes
    }

    /**
     * Keeps a decoration's block entity beside an external block until the target chunk arrives.
     * [expectedStateId] is the block the feature just placed, so a rejected block write cannot
     * leave an orphaned block entity in real neighbour terrain.
     */
    public fun setDecorationBlockEntity(
        worldX: Int,
        y: Int,
        worldZ: Int,
        expectedStateId: Int,
        type: Identifier,
        data: NbtCompound,
    ) {
        if (y !in verticalBounds.minY..verticalBounds.maxY) return
        if (worldX shr 4 == chunkX && worldZ shr 4 == chunkZ) {
            setBlockEntity(ChunkBlockEntity(worldX and 15, y, worldZ and 15, type, data))
            return
        }
        generatedExternalBlockEntityWrites[GeneratedWorldPos(worldX, y, worldZ)] =
            ChunkGeneratedBlockEntityWrite(worldX, y, worldZ, expectedStateId, type, data)
    }

    /** Hands deferred cross-chunk decoration block entities to the runtime exactly once. */
    public fun takeGeneratedBlockEntityWrites(): List<ChunkGeneratedBlockEntityWrite> {
        if (generatedExternalBlockEntityWrites.isEmpty()) return emptyList()
        val writes = generatedExternalBlockEntityWrites.values.toList()
        generatedExternalBlockEntityWrites.clear()
        return writes
    }

    /** Schedules a generated block's tick locally or keeps it beside an external block write. */
    public fun scheduleDecorationBlockTick(
        worldX: Int,
        y: Int,
        worldZ: Int,
        expectedStateId: Int,
        type: Identifier,
        delay: Int,
        priority: Int = 0,
    ) {
        if (y !in verticalBounds.minY..verticalBounds.maxY) return
        if (worldX shr 4 == chunkX && worldZ shr 4 == chunkZ) {
            if (getBlockStateId(worldX and 15, y, worldZ and 15) == expectedStateId) {
                scheduleBlockTick(ChunkScheduledTick(type, worldX and 15, y, worldZ and 15, delay, priority))
            }
            return
        }
        generatedExternalBlockTicks[GeneratedWorldPos(worldX, y, worldZ)] =
            ChunkGeneratedBlockTick(worldX, y, worldZ, expectedStateId, type, delay, priority)
    }

    /** Hands deferred cross-chunk decoration ticks to the runtime exactly once. */
    public fun takeGeneratedBlockTicks(): List<ChunkGeneratedBlockTick> {
        if (generatedExternalBlockTicks.isEmpty()) return emptyList()
        val ticks = generatedExternalBlockTicks.values.toList()
        generatedExternalBlockTicks.clear()
        return ticks
    }

    /**
     * parity: `LevelChunk.setBlockState`, which drops the block entity only when the *block* changes.
     * A furnace lighting up, a campfire being put out or a chest taking on water keeps its own: they
     * are the same block in another state, and what is inside them does not fall out.
     */
    private fun changesBlock(
        oldStateId: Int,
        newStateId: Int,
    ): Boolean {
        if (oldStateId == newStateId) return false
        return BlockStateDefinitions.blockOf(oldStateId) != BlockStateDefinitions.blockOf(newStateId)
    }

    /**
     * Tells the chunk its blocks changed behind its back, which writing through [section] does.
     * The heightmaps are rebuilt on the next read rather than here, so loading a chunk section by
     * section rebuilds once and not once per section.
     */
    public fun invalidateHeightmaps() {
        heightsValid = false
        skyLightSections = null
        blockLightSections = null
    }

    /**
     * This chunk's sky light, packed one nibble array per section, kept until a block moves.
     *
     * A pass run without every neighbour loaded is kept all the same: it lets light in from where
     * nothing is known yet, and [invalidateLight] throws it away as soon as the missing neighbour
     * arrives. Not keeping it meant recomputing the whole chunk on *every* query at the edge of
     * the loaded area, which cost over two milliseconds a time.
     */
    public fun skyLight(compute: () -> Array<ByteArray>): Array<ByteArray> =
        skyLightSections ?: compute().also { skyLightSections = it }

    /** The same for the light the blocks themselves give off. */
    public fun blockLight(compute: () -> Array<ByteArray>): Array<ByteArray> =
        blockLightSections ?: compute().also { blockLightSections = it }

    /**
     * The light this chunk has already worked out, or null when nobody has asked yet.
     *
     * A reader that only wants one block's light uses this to skip building a window over the nine
     * chunks around it: that window was costing more than the lookup it was there to serve.
     */
    public fun cachedSkyLight(): Array<ByteArray>? = skyLightSections

    public fun cachedBlockLight(): Array<ByteArray>? = blockLightSections

    /** Drops the kept light, for when a neighbour arrives and this chunk's own pass is now stale. */
    public fun invalidateLight() {
        skyLightSections = null
        blockLightSections = null
    }

    private var skyLightSections: Array<ByteArray>? = null
    private var blockLightSections: Array<ByteArray>? = null

    public fun setBlockEntity(blockEntity: ChunkBlockEntity) {
        require(blockEntity.y in verticalBounds.minY..verticalBounds.maxY) {
            "block entity y=${blockEntity.y} outside chunk bounds"
        }
        blockEntitiesByPosition[blockEntityKey(blockEntity.localX, blockEntity.y, blockEntity.localZ)] = blockEntity
        isDirty = true
    }

    public fun blockEntityAt(
        localX: Int,
        y: Int,
        localZ: Int,
    ): ChunkBlockEntity? = blockEntitiesByPosition[blockEntityKey(localX, y, localZ)]

    public fun blockEntities(): Collection<ChunkBlockEntity> = blockEntitiesByPosition.values

    /** How many block entities this chunk holds, without copying the collection. */
    public val blockEntityCount: Int get() = blockEntitiesByPosition.size

    /** Scheduled block plus fluid ticks held, without copying either list. */
    public val scheduledTickCount: Int get() = scheduledBlockTicksBacking.size + scheduledFluidTicksBacking.size

    /** Approximate bytes the packed sections and heightmaps keep resident; attachments are counted, not sized. */
    public fun residentBytes(): Long {
        var bytes = heights.size.toLong() * 16 * 16 * Int.SIZE_BYTES
        for (section in sections) bytes += section.residentBytes
        return bytes
    }

    /**
     * The entities this chunk carries in the region file, as opaque NBT.
     *
     * They are opaque on purpose: what an entity is made of belongs to the runtime, and this layer
     * only has to keep it beside the blocks it stands on and hand it back when the chunk returns.
     */
    public fun savedEntities(): List<NbtCompound> = savedEntitiesBacking

    public fun setSavedEntities(entities: List<NbtCompound>) {
        if (savedEntitiesBacking.isEmpty() && entities.isEmpty()) return
        savedEntitiesBacking = entities
        isDirty = true
    }

    /** Hands them over once, which is what stops a chunk loaded twice spawning them twice. */
    public fun takeSavedEntities(): List<NbtCompound> {
        val taken = savedEntitiesBacking
        savedEntitiesBacking = emptyList()
        return taken
    }

    public fun addGeneratedEntity(entity: ChunkGeneratedEntity) {
        generatedEntitiesBacking += entity
        isDirty = true
    }

    public fun generatedEntities(): List<ChunkGeneratedEntity> = generatedEntitiesBacking.toList()

    /** How many [generatedEntities] holds, for readers that only want one and not a copy of the list. */
    public val generatedEntityCount: Int get() = generatedEntitiesBacking.size

    public fun generatedEntityAt(index: Int): ChunkGeneratedEntity = generatedEntitiesBacking[index]

    public fun scheduleBlockTick(tick: ChunkScheduledTick) {
        scheduleTick(scheduledBlockTicksBacking, tick)
    }

    public fun scheduleFluidTick(tick: ChunkScheduledTick) {
        scheduleTick(scheduledFluidTicksBacking, tick)
    }

    public fun scheduledBlockTicks(): List<ChunkScheduledTick> = scheduledBlockTicksBacking.toList()

    public fun scheduledFluidTicks(): List<ChunkScheduledTick> = scheduledFluidTicksBacking.toList()

    /**
     * Counts every pending tick down one and hands the ones that come due to [due].
     *
     * The countdown itself does not dirty the chunk: whatever the tick then does to a block will,
     * and marking it every tick would have a single flowing spring rewriting its region file
     * twenty times a second.
     */
    public fun advanceBlockTicks(due: MutableList<ChunkScheduledTick>) {
        advance(scheduledBlockTicksBacking, due)
    }

    public fun advanceFluidTicks(due: MutableList<ChunkScheduledTick>) {
        advance(scheduledFluidTicksBacking, due)
    }

    private fun advance(
        pending: MutableList<ChunkScheduledTick>,
        due: MutableList<ChunkScheduledTick>,
    ) {
        if (pending.isEmpty()) return
        var index = 0
        while (index < pending.size) {
            val tick = pending[index]
            if (tick.delay <= 1) {
                due += tick
                pending.removeAt(index)
            } else {
                pending[index] = tick.copy(delay = tick.delay - 1)
                index++
            }
        }
    }

    private fun scheduleTick(
        ticks: MutableList<ChunkScheduledTick>,
        tick: ChunkScheduledTick,
    ) {
        require(tick.y in verticalBounds.minY..verticalBounds.maxY) {
            "scheduled tick y=${tick.y} outside chunk bounds"
        }
        val duplicate =
            ticks.any {
                it.type == tick.type && it.localX == tick.localX && it.y == tick.y && it.localZ == tick.localZ
            }
        if (!duplicate) {
            ticks += tick
            isDirty = true
        }
    }

    private data class GeneratedWorldPos(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    /** Synchronously records block placement attempts without allocating on the normal write path. */
    internal fun <T> observeBlockWrites(
        observer: (x: Int, y: Int, z: Int, oldStateId: Int, newStateId: Int) -> Unit,
        action: () -> T,
    ): T {
        check(blockWriteObserver == null) { "nested block write observers are not supported" }
        blockWriteObserver = observer
        try {
            return action()
        } finally {
            blockWriteObserver = null
        }
    }

    private fun sectionFor(y: Int): ChunkSection {
        require(y in verticalBounds.minY..verticalBounds.maxY) { "y=$y out of world bounds" }
        return sections[(y shr 4) - verticalBounds.minSection]
    }

    private fun blockEntityKey(
        x: Int,
        y: Int,
        z: Int,
    ): Int = ((y - verticalBounds.minY) shl 8) or ((z and 15) shl 4) or (x and 15)

    /**
     * Sets the biome network ID at quart (4-block) resolution. [quartY] is the world quart
     * (0 until `verticalBounds.height / 4`), [quartX]/[quartZ] chunk-local (0..3).
     */
    public fun setBiomeId(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        biomeId: Int,
    ) {
        require(quartY in 0 until verticalBounds.height / 4) { "quartY=$quartY out of world bounds" }
        sections[quartY shr 2].setBiomeId(quartX, quartY and 3, quartZ, biomeId)
        isDirty = true
    }

    /** The biome network id at one quart, in the same coordinates [setBiomeId] takes. */
    public fun getBiomeId(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): Int {
        require(quartY in 0 until verticalBounds.height / 4) { "quartY=$quartY out of world bounds" }
        return sections[quartY shr 2].getBiomeId(quartX, quartY and 3, quartZ)
    }

    /** Topmost matching Y plus one per column, matching vanilla's heightmap convention. */
    public fun heightmap(type: HeightmapType = HeightmapType.MOTION_BLOCKING): IntArray {
        if (!heightsValid) rebuildHeights()
        return heights[type.ordinal].copyOf()
    }

    /** The heightmap value for one local column, without allocating/scanning the other 255. */
    public fun heightAt(
        x: Int,
        z: Int,
        type: HeightmapType = HeightmapType.MOTION_BLOCKING,
    ): Int {
        if (!heightsValid) rebuildHeights()
        return heights[type.ordinal][x + z * 16]
    }

    /**
     * // parity: Heightmap.update. A block at or above the recorded height raises it; removing the
     * one *at* it is the only case that has to look downwards, and only within that column.
     */
    private fun updateHeights(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ) {
        skyLightSections = null
        blockLightSections = null
        if (!heightsValid) return
        val column = (x and 15) + (z and 15) * 16
        for (i in HEIGHTMAP_TYPES.indices) {
            val type = HEIGHTMAP_TYPES[i]
            val map = heights[i]
            val current = map[column]
            if (type.matches(stateId)) {
                if (y >= current) map[column] = y + 1
            } else if (y == current - 1) {
                var scan = y - 1
                while (scan >= verticalBounds.minY && !type.matches(getBlockStateId(x, scan, z))) scan--
                map[column] = scan + 1
            }
        }
    }

    /** All four heightmaps in one downward pass per column, skipping the sections that hold only air. */
    private fun rebuildHeights() {
        val allPending = (1 shl HEIGHTMAP_TYPES.size) - 1
        for (i in HEIGHTMAP_TYPES.indices) heights[i].fill(verticalBounds.minY)
        for (z in 0..15) {
            for (x in 0..15) {
                val column = x + z * 16
                var pending = allPending
                var y = verticalBounds.maxY
                var steps = 0
                while (y >= verticalBounds.minY && pending != 0) {
                    val section = sections[(y shr 4) - verticalBounds.minSection]
                    if (section.nonAirBlockCount == 0) {
                        y = (y and 15.inv()) - 1
                        continue
                    }
                    val stateId = section.getBlockStateId(x, y and 15, z)
                    steps++
                    for (i in HEIGHTMAP_TYPES.indices) {
                        if (pending and (1 shl i) == 0) continue
                        if (!HEIGHTMAP_TYPES[i].matches(stateId)) continue
                        heights[i][column] = y + 1
                        pending = pending and (1 shl i).inv()
                    }
                    y--
                }
                if (GenerationMetrics.enabled) GenerationMetrics.countHeightmapScan(steps)
            }
        }
        heightsValid = true
    }

    private companion object {
        /** Indexed by ordinal, so the write path walks an array instead of an enum iterator. */
        val HEIGHTMAP_TYPES: Array<HeightmapType> = HeightmapType.entries.toTypedArray()
    }

    public enum class HeightmapType {
        /** // parity: Heightmap.Types.WORLD_SURFACE(_WG), every non-air block and fluid. */
        WORLD_SURFACE,

        /** // parity: Heightmap.Types.OCEAN_FLOOR(_WG), blocks that stop motion but not fluids. */
        OCEAN_FLOOR,

        /** // parity: Heightmap.Types.MOTION_BLOCKING, motion-blocking blocks plus fluids. */
        MOTION_BLOCKING,

        /** // parity: MOTION_BLOCKING_NO_LEAVES, used by near-water firefly bushes. */
        MOTION_BLOCKING_NO_LEAVES,
        ;

        /**
         * A precomputed answer per block state rather than the predicate chain below, which costs a
         * set lookup plus a dozen `Blocks.isXState` calls every time. Heightmap scans run this
         * millions of times per chunk (see [GenerationMetrics]' own scan-step counter).
         *
         * The table is built once when the companion initializes, not through `by lazy`: `lazy`
         * defaults to [LazyThreadSafetyMode.SYNCHRONIZED] and takes its lock on every read, which
         * measured *slower* than the predicate chain it was meant to replace, by two orders of
         * magnitude, once several million reads per chunk went through it.
         */
        public fun matches(stateId: Int): Boolean =
            if (stateId in 0 until TABLE_SIZE) TABLES[ordinal][stateId] else matchesUncached(stateId)

        private fun matchesUncached(stateId: Int): Boolean =
            when (this) {
                WORLD_SURFACE -> !Blocks.isAir(stateId)
                OCEAN_FLOOR -> BlockShapes.blocksMotion(stateId)
                MOTION_BLOCKING -> BlockShapes.blocksMotion(stateId) || Blocks.hasFluid(stateId)
                MOTION_BLOCKING_NO_LEAVES ->
                    !Blocks.isLeavesState(stateId) &&
                        (BlockShapes.blocksMotion(stateId) || Blocks.hasFluid(stateId))
            }

        private companion object {
            // The bound the tables were actually sized with, so a state id past it (nothing
            // registers derived states after startup today, but nothing enforces that either)
            // falls back to the uncached path instead of reading off the end.
            val TABLE_SIZE: Int = Blocks.stateCount

            // Declared last because companion properties initialize in declaration order.
            val TABLES: Array<BooleanArray> =
                Array(entries.size) { type -> BooleanArray(TABLE_SIZE) { entries[type].matchesUncached(it) } }
        }
    }
}
