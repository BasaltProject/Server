package org.basaltmc.world

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier

/** A 16-wide column of blocks at ([chunkX], [chunkZ]) in chunk coordinates. */
public class Chunk(
    public val chunkX: Int,
    public val chunkZ: Int,
    public val verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
) {
    private val sections = Array(verticalBounds.sectionCount) { ChunkSection() }
    private val blockEntitiesByPosition = LinkedHashMap<Int, ChunkBlockEntity>()
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
            if (section.setBlockStateId(x, y and 15, z, stateId)) isDirty = true
        } else {
            val oldStateId = section.getBlockStateId(x, y and 15, z)
            section.setBlockStateId(x, y and 15, z, stateId)
            if (oldStateId != stateId) {
                isDirty = true
                blockEntitiesByPosition.remove(blockEntityKey(x, y, z))
                observer(x, y, z, oldStateId, stateId)
            }
        }
    }

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

    /** Topmost matching Y plus one per column, matching vanilla's heightmap convention. */
    public fun heightmap(type: HeightmapType = HeightmapType.MOTION_BLOCKING): IntArray =
        IntArray(16 * 16) { i ->
            val x = i and 15
            val z = i shr 4
            heightAt(x, z, type)
        }

    /** The heightmap value for one local column, without allocating/scanning the other 255. */
    public fun heightAt(
        x: Int,
        z: Int,
        type: HeightmapType = HeightmapType.MOTION_BLOCKING,
    ): Int {
        var y = verticalBounds.maxY
        while (y >= verticalBounds.minY && !type.matches(getBlockStateId(x, y, z))) y--
        if (GenerationMetrics.enabled) GenerationMetrics.countHeightmapScan(verticalBounds.maxY - y)
        return y + 1
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
                WORLD_SURFACE -> stateId != Blocks.AIR.stateId
                OCEAN_FLOOR ->
                    stateId != Blocks.AIR.stateId &&
                        stateId != Blocks.WATER.stateId &&
                        !isNonColliding(stateId)
                MOTION_BLOCKING ->
                    Blocks.hasFluid(stateId) ||
                        (stateId != Blocks.AIR.stateId && !isNonColliding(stateId))
                MOTION_BLOCKING_NO_LEAVES ->
                    !Blocks.isLeavesState(stateId) &&
                        (
                            Blocks.hasFluid(stateId) ||
                                (stateId != Blocks.AIR.stateId && !isNonColliding(stateId))
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
