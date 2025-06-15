package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

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
    private val scheduledBlockTicksBacking = mutableListOf<ChunkScheduledTick>()
    private val scheduledFluidTicksBacking = mutableListOf<ChunkScheduledTick>()
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
            if (blockEntitiesByPosition.isNotEmpty() && section.getBlockStateId(x, y and 15, z) != stateId) {
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
                blockEntitiesByPosition.remove(blockEntityKey(x, y, z))
                observer(x, y, z, oldStateId, stateId)
            }
        }
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
     * This chunk's sky light, packed one nibble array per section, computed once and kept until a
     * block moves. Vanilla keeps light the same way, and for the same reason: recomputing it per
     * player who is sent the chunk is the same work several times over.
     */
    public fun skyLight(compute: () -> Array<ByteArray>): Array<ByteArray> =
        skyLightSections ?: compute().also { skyLightSections = it }

    /** The same for the light the blocks themselves give off. */
    public fun blockLight(compute: () -> Array<ByteArray>): Array<ByteArray> =
        blockLightSections ?: compute().also { blockLightSections = it }

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

    /** Synchronously records changed block writes without allocating on the normal write path. */
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
                OCEAN_FLOOR ->
                    !Blocks.isAir(stateId) &&
                        stateId != Blocks.WATER.stateId &&
                        !isNonColliding(stateId)
                MOTION_BLOCKING ->
                    Blocks.hasFluid(stateId) ||
                        (!Blocks.isAir(stateId) && !isNonColliding(stateId))
                MOTION_BLOCKING_NO_LEAVES ->
                    !Blocks.isLeavesState(stateId) &&
                        (
                            Blocks.hasFluid(stateId) ||
                                (!Blocks.isAir(stateId) && !isNonColliding(stateId))
                        )
            }

        private companion object {
            fun isNonColliding(stateId: Int): Boolean =
                stateId in NON_COLLIDING_STATE_IDS ||
                    Blocks.isGlowLichenState(stateId) ||
                    Blocks.isVineState(stateId) ||
                    Blocks.isCocoaState(stateId) ||
                    Blocks.isPaleHangingMossState(stateId) ||
                    Blocks.isMangrovePropaguleState(stateId) ||
                    Blocks.isLeafLitterState(stateId) ||
                    Blocks.isDoublePlantState(stateId) ||
                    Blocks.isWildflowersState(stateId) ||
                    Blocks.isPinkPetalsState(stateId) ||
                    Blocks.isKelpState(stateId) ||
                    Blocks.isSeaPickleState(stateId) ||
                    Blocks.isCoralPlantState(stateId)

            val NON_COLLIDING_STATE_IDS: Set<Int> =
                listOf(
                    "short_grass",
                    "tall_grass",
                    "bush",
                    "dandelion",
                    "poppy",
                    "azure_bluet",
                    "oxeye_daisy",
                    "cornflower",
                    "orange_tulip",
                    "red_tulip",
                    "pink_tulip",
                    "white_tulip",
                    "seagrass",
                    "tall_seagrass",
                    "kelp",
                    "kelp_plant",
                    "sea_pickle",
                    "tube_coral",
                    "brain_coral",
                    "bubble_coral",
                    "fire_coral",
                    "horn_coral",
                    "tube_coral_fan",
                    "brain_coral_fan",
                    "bubble_coral_fan",
                    "fire_coral_fan",
                    "horn_coral_fan",
                    "tube_coral_wall_fan",
                    "brain_coral_wall_fan",
                    "bubble_coral_wall_fan",
                    "fire_coral_wall_fan",
                    "horn_coral_wall_fan",
                ).mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()

            // The bound the tables were actually sized with, so a state id past it (nothing
            // registers derived states after startup today, but nothing enforces that either)
            // falls back to the uncached path instead of reading off the end.
            val TABLE_SIZE: Int = Blocks.stateCount

            // Declared last: reads NON_COLLIDING_STATE_IDS and TABLE_SIZE, and companion properties
            // initialize in declaration order.
            val TABLES: Array<BooleanArray> =
                Array(entries.size) { type -> BooleanArray(TABLE_SIZE) { entries[type].matchesUncached(it) } }
        }
    }
}
