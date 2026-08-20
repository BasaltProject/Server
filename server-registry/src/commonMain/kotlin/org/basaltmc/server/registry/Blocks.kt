package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** A single block state, identified by its vanilla network state ID. */
public data class BlockState(
    public val block: Identifier,
    public val stateId: Int,
    public val properties: Map<String, String> = emptyMap(),
)

/**
 * Every block's default state, regenerated straight from the real 26.2 server jar's own data
 * generator output (see [ALL_DEFAULT_BLOCK_STATES]'s KDoc): authoritative state IDs, not
 * decompiled or guessed. Registry entries expose one default state per block; worldgen derives
 * contiguous non-default state ranges only for the few property-bearing blocks it writes directly.
 * A handful of commonly referenced blocks get named constants for readability; every other block
 * remains in [REGISTRY] without its own named `val`.
 */
public object Blocks {
    public val REGISTRY: Registry<BlockState> = Registry()

    public val AIR: BlockState = register(0, "air")
    public val STONE: BlockState = register(1, "stone")
    public val GRASS_BLOCK: BlockState = register(9, "grass_block")
    public val DIRT: BlockState = register(10, "dirt")
    public val PODZOL: BlockState = register(13, "podzol")
    public val BEDROCK: BlockState = register(85, "bedrock")
    public val WATER: BlockState = register(86, "water")
    public val LAVA: BlockState = register(102, "lava")
    public val OAK_LOG: BlockState = register(137, "oak_log")
    public val OAK_LEAVES: BlockState = register(279, "oak_leaves")
    public val SEAGRASS: BlockState = register(2254, "seagrass")
    public val TALL_SEAGRASS: BlockState = register(2256, "tall_seagrass")
    public val KELP: BlockState = register(15062, "kelp")
    public val KELP_PLANT: BlockState = register(15088, "kelp_plant")
    public val SEA_PICKLE: BlockState = register(15267, "sea_pickle")
    public val BAMBOO: BlockState = register(15279, "bamboo")
    public val SNOW: BlockState = register(6919, "snow")
    public val ICE: BlockState = register(6927, "ice")
    public val SNOW_BLOCK: BlockState = register(6928, "snow_block")
    public val PACKED_ICE: BlockState = register(12914, "packed_ice")
    public val BLUE_ICE: BlockState = register(15275, "blue_ice")
    public val PUMPKIN: BlockState = register(8332, "pumpkin")
    public val SUGAR_CANE: BlockState = register(6947, "sugar_cane")
    public val OBSIDIAN: BlockState = register(3369, "obsidian")
    public val FIRE: BlockState = register(3406, "fire")
    public val IRON_BARS: BlockState = register(7989, "iron_bars")
    public val FIREFLY_BUSH: BlockState = register(32365, "firefly_bush")
    public val VOID_AIR: BlockState = register(15292, "void_air")
    public val CAVE_AIR: BlockState = register(15293, "cave_air")
    public val NETHERRACK: BlockState = register(6997, "netherrack")
    public val END_STONE: BlockState = register(9477, "end_stone")
    public val END_GATEWAY: BlockState = register(14816, "end_gateway")

    init {
        for ((stateId, path) in ALL_DEFAULT_BLOCK_STATES) {
            if (REGISTRY.byId(stateId) == null) register(stateId, path)
        }
    }

    /**
     * How long a block takes to break and whether it drops anything without the right tool.
     *
     * A [destroyTime] of zero means it comes apart instantly, which matters more than it sounds: a
     * client destroys those on the spot and only ever sends the *start* of the dig, never the end,
     * so a server that waits for the end never breaks a flower. Below zero means unbreakable.
     */
    public data class BlockHardness(
        public val destroyTime: Float,
        public val requiresCorrectTool: Boolean,
        /** parity: `Block.getExplosionResistance`, what an explosion has to spend to get through. */
        public val explosionResistance: Float = 0f,
    ) {
        public val isInstant: Boolean get() = destroyTime == 0f
        public val isUnbreakable: Boolean get() = destroyTime < 0f
    }

    private val hardnessByBlock: Map<Identifier, BlockHardness> =
        ALL_BLOCK_HARDNESS.associate { entry ->
            Identifier.vanilla(entry.path) to
                BlockHardness(entry.destroyTime, entry.requiresCorrectTool, entry.explosionResistance)
        }

    /**
     * Every block's id in the `minecraft:block` registry, which is **not** its block state id.
     *
     * Tags name blocks, so their members travel as these; chunk data and block updates carry state
     * ids instead. The two are different numbers for every block past the first.
     */
    private val blockIdsByName: Map<Identifier, Int> =
        ALL_BLOCK_IDS.associate { (id, path) -> Identifier.vanilla(path) to id }

    /** This block's `minecraft:block` registry id, or `null` if it is not a known block. */
    public fun blockIdOf(block: Identifier): Int? = blockIdsByName[block]

    /** This block's destroy time and tool requirement, or `null` for one this build's table does not list. */
    public fun hardnessOf(block: Identifier): BlockHardness? = hardnessByBlock[block]

    /**
     * The block state with this id, whatever it is.
     *
     * The two tables only hold default states and the ones worldgen writes, so anything else was
     * answered as "not a block": a copper chest facing anywhere was nothing the interaction code
     * could see, and clicking it did nothing at all. The state definition knows every combination,
     * so it answers the rest.
     */
    public fun byStateId(stateId: Int): BlockState? {
        derivedStatesById[stateId]?.let { return it }
        // A registry entry is one block's default state and carries no properties with it, so a
        // question about a property (`/execute if block ... redstone_torch[lit=true]`) was answered
        // "no" for every block sitting in its own default state. The definition has them all.
        val defined =
            BlockStateDefinitions.blockOf(stateId)?.let {
                BlockState(it, stateId, BlockStateDefinitions.propertiesOf(stateId))
            }
        if (defined != null && defined.properties.isNotEmpty()) return defined
        return REGISTRY.byId(stateId) ?: defined
    }

    /**
     * Resolves a block plus its serialized vanilla properties back to a network state ID.
     *
     * The table only holds the combinations worldgen writes, so anything else is worked out from the
     * block's property definition, taking the default state's values for whatever was left unsaid.
     * parity: `BlockStateParser`, which reads a partial state the same way.
     */
    public fun stateIdOf(
        block: Identifier,
        properties: Map<String, String>,
    ): Int? {
        val default = REGISTRY.idOf(block) ?: return null
        if (properties.isEmpty()) return default
        return derivedStateIds[StateKey(block, properties)]
            ?: BlockStateDefinitions.stateIdOf(block, properties, default)
    }

    /**
     * Number of IDs required by vanilla's direct block-state palette. Computed once and kept cheap:
     * as a getter scanning [derivedStatesById] it read like a field and cost O(n), which measured
     * three orders of magnitude on a per-block path.
     *
     * [LazyThreadSafetyMode.PUBLICATION] because the default takes its lock on every read, not
     * just the first; the computation is deterministic, so a race costs one duplicate.
     */
    public val stateCount: Int by lazy(LazyThreadSafetyMode.PUBLICATION) {
        maxOf(ALL_DEFAULT_BLOCK_STATES.last().first, derivedStatesById.keys.maxOrNull() ?: 0) + 1
    }

    /** Whether this default block state exposes a non-empty water fluid state. */
    public fun hasWaterFluid(stateId: Int): Boolean {
        // Every level of the water block, not just the source: flowing water is water, and reading
        // it as anything else left it out of the section's fluid count and out of the flow engine.
        if (stateId in WATER.stateId..WATER.stateId + 15) return true
        if (stateId in WATER_FLUID_STATE_IDS) return true
        if (
            isGlowLichenState(stateId) &&
            ((stateId - GLOW_LICHEN_FIRST_STATE_ID) and GLOW_LICHEN_WATERLOGGED_FALSE_BIT) == 0
        ) {
            return true
        }
        if (
            isMangrovePropaguleState(stateId) &&
            ((stateId - MANGROVE_PROPAGULE_FIRST_STATE_ID) and 1) == 0
        ) {
            return true
        }
        if (isLeavesState(stateId) && stateId and 1 == 0) return true
        if (isKelpState(stateId) || stateId in TALL_SEAGRASS_UPPER_STATE_ID..TALL_SEAGRASS.stateId) return true
        if (isSeaPickleState(stateId) && (stateId - SEA_PICKLE.stateId) and 1 == 0) return true
        val coralStateStart = CORAL_WATERLOGGED_DEFAULT_STATE_IDS.firstOrNull { stateId in it..it + 1 }
        if (coralStateStart != null) return (stateId - coralStateStart) and 1 == 0
        val wallCoralStateStart = CORAL_WALL_FAN_DEFAULT_STATE_IDS.firstOrNull { stateId in it..it + 7 }
        if (wallCoralStateStart != null) return (stateId - wallCoralStateStart) and 1 == 0
        return derivedStatesById[stateId]?.properties?.get("waterlogged") == "true"
    }

    /** Whether this state exposes any non-empty fluid state. */
    public fun hasFluid(stateId: Int): Boolean = stateId in LAVA.stateId..LAVA.stateId + 15 || hasWaterFluid(stateId)

    /** // parity: BlockState.isAir, including the two non-default air blocks used by worldgen. */
    public fun isAir(stateId: Int): Boolean =
        stateId == AIR.stateId || stateId == VOID_AIR.stateId || stateId == CAVE_AIR.stateId

    /** Whether [stateId] is one of the 28 distance, persistence, and water states of a leaves block. */
    public fun isLeavesState(stateId: Int): Boolean = stateId in LEAVES_FIRST_STATE_ID..LEAVES_LAST_STATE_ID

    /** Returns the vanilla distance property for a leaves state, or null for any other block. */
    public fun leafDistance(stateId: Int): Int? =
        if (isLeavesState(stateId)) ((stateId - leafStateRangeStart(stateId)) / 4) + 1 else null

    /** Changes only the distance property of a leaves state. */
    public fun withLeafDistance(
        stateId: Int,
        distance: Int,
    ): Int? {
        require(distance in 1..7)
        if (!isLeavesState(stateId)) return null
        return leafStateRangeStart(stateId) + (distance - 1) * 4 + (stateId and 3)
    }

    /** Whether [stateId] is an upper or lower half of a worldgen double plant. */
    public fun isDoublePlantState(stateId: Int): Boolean =
        DOUBLE_PLANT_DEFAULT_STATE_IDS.any { defaultStateId -> stateId in defaultStateId - 1..defaultStateId }

    /** Returns the upper-half state matching a lower double-plant state. */
    public fun upperDoublePlantStateId(lowerStateId: Int): Int? =
        if (lowerStateId in DOUBLE_PLANT_DEFAULT_STATE_IDS) lowerStateId - 1 else null

    /** Returns the matching snowy or bare state of grass block, podzol, or mycelium. */
    public fun withSnowy(
        stateId: Int,
        snowy: Boolean,
    ): Int? {
        val defaultStateId =
            SNOWY_BLOCK_DEFAULT_STATE_IDS.firstOrNull { candidate -> stateId in candidate - 1..candidate }
                ?: return null
        return if (snowy) defaultStateId - 1 else defaultStateId
    }

    /** Whether [stateId] is one of kelp's 26 age states. */
    public fun isKelpState(stateId: Int): Boolean = stateId in KELP.stateId..KELP.stateId + 25

    /** Returns a twisting/weeping vine head state with vanilla's requested age. */
    public fun growingVineHeadStateId(
        stateId: Int,
        age: Int,
    ): Int? {
        require(age in 0..25)
        val defaultStateId = GROWING_VINE_HEAD_DEFAULT_STATE_IDS.firstOrNull { stateId in it..it + 25 } ?: return null
        return defaultStateId + age
    }

    /** Whether [stateId] is one of sea pickle's count and water states. */
    public fun isSeaPickleState(stateId: Int): Boolean = stateId in SEA_PICKLE.stateId..SEA_PICKLE.stateId + 7

    /** Whether [stateId] is a live coral plant, floor fan, or wall fan state. */
    public fun isCoralPlantState(stateId: Int): Boolean =
        CORAL_WATERLOGGED_DEFAULT_STATE_IDS.any { stateId in it..it + 1 } ||
            CORAL_WALL_FAN_DEFAULT_STATE_IDS.any { stateId in it..it + 7 }

    /** Returns a wall coral fan state with the requested horizontal facing and water state. */
    public fun coralWallFanStateId(
        stateId: Int,
        facing: String,
        waterlogged: Boolean,
    ): Int? {
        val defaultStateId =
            CORAL_WALL_FAN_DEFAULT_STATE_IDS.firstOrNull { candidate -> stateId in candidate..candidate + 7 }
                ?: return null
        val facingOffset = HORIZONTAL_FACING_OFFSETS[facing] ?: return null
        return defaultStateId + facingOffset * 2 + if (waterlogged) 0 else 1
    }

    /** Whether [stateId] is one of a huge mushroom cap or stem block's 64 face states. */
    public fun isHugeMushroomState(stateId: Int): Boolean =
        HUGE_MUSHROOM_DEFAULT_STATE_IDS.any { stateId in it..it + 63 }

    /** Returns a huge mushroom block state with the requested visible faces. */
    public fun hugeMushroomStateId(
        stateId: Int,
        down: Boolean,
        east: Boolean,
        north: Boolean,
        south: Boolean,
        up: Boolean,
        west: Boolean,
    ): Int? {
        val defaultStateId =
            HUGE_MUSHROOM_DEFAULT_STATE_IDS.firstOrNull { candidate -> stateId in candidate..candidate + 63 }
                ?: return null
        var offset = 0
        if (!down) offset += 32
        if (!east) offset += 16
        if (!north) offset += 8
        if (!south) offset += 4
        if (!up) offset += 2
        if (!west) offset++
        return defaultStateId + offset
    }

    /** Whether [stateId] is one of wildflowers' 16 facing and amount states. */
    public fun isWildflowersState(stateId: Int): Boolean =
        stateId in WILDFLOWERS_DEFAULT_STATE_ID..WILDFLOWERS_DEFAULT_STATE_ID + 15

    /** Whether [stateId] is one of pink petals' 16 facing and amount states. */
    public fun isPinkPetalsState(stateId: Int): Boolean =
        stateId in PINK_PETALS_DEFAULT_STATE_ID..PINK_PETALS_DEFAULT_STATE_ID + 15

    /** Whether [stateId] is one of glow lichen's 128 property combinations. */
    public fun isGlowLichenState(stateId: Int): Boolean =
        stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID

    /** Whether [stateId] is one of vine's 32 face-property combinations. */
    public fun isVineState(stateId: Int): Boolean = stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID

    /** Whether [stateId] is one of cocoa's age and facing combinations. */
    public fun isCocoaState(stateId: Int): Boolean = stateId in COCOA_DEFAULT_STATE_ID..COCOA_LAST_STATE_ID

    /** Whether [stateId] is one of pale hanging moss's tip states. */
    public fun isPaleHangingMossState(stateId: Int): Boolean =
        stateId in PALE_HANGING_MOSS_DEFAULT_STATE_ID..PALE_HANGING_MOSS_LAST_STATE_ID

    /** Whether [stateId] is one of creaking heart's axis, lifecycle, and natural combinations. */
    public fun isCreakingHeartState(stateId: Int): Boolean =
        stateId in CREAKING_HEART_FIRST_STATE_ID..CREAKING_HEART_LAST_STATE_ID

    /** Whether [stateId] is one of mangrove propagule's age, hanging, stage, and waterlogged combinations. */
    public fun isMangrovePropaguleState(stateId: Int): Boolean =
        stateId in MANGROVE_PROPAGULE_FIRST_STATE_ID..MANGROVE_PROPAGULE_LAST_STATE_ID

    /** Whether [stateId] is one of leaf litter's facing and segment amount combinations. */
    public fun isLeafLitterState(stateId: Int): Boolean =
        stateId in LEAF_LITTER_DEFAULT_STATE_ID..LEAF_LITTER_LAST_STATE_ID

    /** Whether [stateId] is one of bee nest's facing and honey-level combinations. */
    public fun isBeeNestState(stateId: Int): Boolean = stateId in BEE_NEST_DEFAULT_STATE_ID..BEE_NEST_LAST_STATE_ID

    /** Returns a chorus plant state with the six exact vanilla connection properties. */
    public fun chorusPlantStateId(
        down: Boolean,
        east: Boolean,
        north: Boolean,
        south: Boolean,
        up: Boolean,
        west: Boolean,
    ): Int {
        var enabledBits = 0
        if (down) enabledBits += 32
        if (east) enabledBits += 16
        if (north) enabledBits += 8
        if (south) enabledBits += 4
        if (up) enabledBits += 2
        if (west) enabledBits++
        return CHORUS_PLANT_DEFAULT_STATE_ID - enabledBits
    }

    /** Returns the chorus flower state for vanilla age 0 through 5. */
    public fun chorusFlowerStateId(age: Int): Int {
        require(age in 0..5)
        return CHORUS_FLOWER_DEFAULT_STATE_ID + age
    }

    /** Returns a dry iron-bars state with the exact vanilla horizontal connections. */
    public fun ironBarsStateId(
        north: Boolean,
        east: Boolean,
        south: Boolean,
        west: Boolean,
    ): Int {
        var enabledBits = 0
        if (east) enabledBits += 16
        if (north) enabledBits += 8
        if (south) enabledBits += 4
        if (west) enabledBits++
        return IRON_BARS.stateId - enabledBits
    }

    private fun register(
        stateId: Int,
        path: String,
    ): BlockState {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(stateId, identifier, BlockState(identifier, stateId))
    }

    private val WATER_FLUID_STATE_IDS: Set<Int> =
        listOf(
            "water",
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
        ).mapNotNull { REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()

    private val GLOW_LICHEN_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("glow_lichen"))?.stateId ?: error("unknown block 'glow_lichen'")
    private val GLOW_LICHEN_FIRST_STATE_ID: Int = GLOW_LICHEN_DEFAULT_STATE_ID - 127
    private const val GLOW_LICHEN_WATERLOGGED_FALSE_BIT: Int = 2
    private val VINE_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("vine"))?.stateId ?: error("unknown block 'vine'")
    private val VINE_FIRST_STATE_ID: Int = VINE_DEFAULT_STATE_ID - 31
    private val COCOA_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("cocoa"))?.stateId ?: error("unknown block 'cocoa'")
    private val COCOA_LAST_STATE_ID: Int = COCOA_DEFAULT_STATE_ID + 11
    private val PALE_HANGING_MOSS_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss"))?.stateId
            ?: error("unknown block 'pale_hanging_moss'")
    private val PALE_HANGING_MOSS_LAST_STATE_ID: Int = PALE_HANGING_MOSS_DEFAULT_STATE_ID + 1
    private val CREAKING_HEART_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart"))?.stateId ?: error("unknown block 'creaking_heart'")
    private val CREAKING_HEART_FIRST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID - 7
    private val CREAKING_HEART_LAST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID + 10
    private val MANGROVE_PROPAGULE_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule"))?.stateId
            ?: error("unknown block 'mangrove_propagule'")
    private val MANGROVE_PROPAGULE_FIRST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID - 5
    private val MANGROVE_PROPAGULE_LAST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID + 34
    private val LEAF_LITTER_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter"))?.stateId ?: error("unknown block 'leaf_litter'")
    private val LEAF_LITTER_LAST_STATE_ID: Int = LEAF_LITTER_DEFAULT_STATE_ID + 15
    private val BEE_NEST_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("bee_nest"))?.stateId ?: error("unknown block 'bee_nest'")
    private val BEE_NEST_LAST_STATE_ID: Int = BEE_NEST_DEFAULT_STATE_ID + 23
    private val CHORUS_PLANT_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("chorus_plant"))?.stateId ?: error("unknown block 'chorus_plant'")
    private val CHORUS_PLANT_FIRST_STATE_ID: Int = CHORUS_PLANT_DEFAULT_STATE_ID - 63
    private val CHORUS_FLOWER_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("chorus_flower"))?.stateId ?: error("unknown block 'chorus_flower'")
    private val DOUBLE_PLANT_BLOCKS: List<String> =
        listOf("sunflower", "lilac", "rose_bush", "peony", "tall_grass", "large_fern", "tall_seagrass")
    private val DOUBLE_PLANT_DEFAULT_STATE_IDS: IntArray =
        DOUBLE_PLANT_BLOCKS
            .map { path ->
                REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
            }.toIntArray()
    private val SNOWY_BLOCK_DEFAULT_STATE_IDS: IntArray =
        listOf("grass_block", "podzol", "mycelium")
            .map { path ->
                REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
            }.toIntArray()
    private val CORAL_WATERLOGGED_DEFAULT_STATE_IDS: IntArray =
        listOf(
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
        ).map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val CORAL_WALL_FAN_DEFAULT_STATE_IDS: IntArray =
        listOf(
            "tube_coral_wall_fan",
            "brain_coral_wall_fan",
            "bubble_coral_wall_fan",
            "fire_coral_wall_fan",
            "horn_coral_wall_fan",
        ).map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val HUGE_MUSHROOM_DEFAULT_STATE_IDS: IntArray =
        listOf("brown_mushroom_block", "red_mushroom_block", "mushroom_stem")
            .map { path ->
                REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
            }.toIntArray()
    private val TALL_SEAGRASS_UPPER_STATE_ID: Int = TALL_SEAGRASS.stateId - 1
    private val GROWING_VINE_HEAD_DEFAULT_STATE_IDS: IntArray =
        listOf("weeping_vines", "twisting_vines")
            .map { path ->
                REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
            }.toIntArray()
    private val WILDFLOWERS_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("wildflowers"))?.stateId ?: error("unknown block 'wildflowers'")
    private val PINK_PETALS_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("pink_petals"))?.stateId ?: error("unknown block 'pink_petals'")
    private val LEAF_BLOCKS: List<String> =
        listOf(
            "oak_leaves",
            "spruce_leaves",
            "birch_leaves",
            "jungle_leaves",
            "acacia_leaves",
            "cherry_leaves",
            "dark_oak_leaves",
            "pale_oak_leaves",
            "mangrove_leaves",
            "azalea_leaves",
            "flowering_azalea_leaves",
        )
    private val LEAVES_FIRST_STATE_ID: Int = OAK_LEAVES.stateId - 27
    private val LEAVES_LAST_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("flowering_azalea_leaves"))?.stateId
            ?: error("unknown block 'flowering_azalea_leaves'")

    private fun leafStateRangeStart(stateId: Int): Int =
        LEAVES_FIRST_STATE_ID + ((stateId - LEAVES_FIRST_STATE_ID) / 28) * 28

    private data class StateKey(
        val block: Identifier,
        val properties: Map<String, String>,
    )

    private val derivedStatesById: Map<Int, BlockState> by lazy {
        buildMap {
            for (path in AXIS_BLOCKS) {
                val default = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
                put(default.stateId - 1, BlockState(default.block, default.stateId - 1, mapOf("axis" to "x")))
                put(default.stateId, BlockState(default.block, default.stateId, mapOf("axis" to "y")))
                put(default.stateId + 1, BlockState(default.block, default.stateId + 1, mapOf("axis" to "z")))
            }

            for (path in AMETHYST_CLUSTER_BLOCKS) {
                val default = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
                var stateId = default.stateId - 9
                for (facing in listOf("north", "east", "south", "west", "up", "down")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            stateId,
                            BlockState(
                                default.block,
                                stateId,
                                linkedMapOf("facing" to facing, "waterlogged" to waterlogged),
                            ),
                        )
                        stateId++
                    }
                }
            }

            for (path in SPELEOTHEM_BLOCKS) {
                val default = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: continue
                var stateId = default.stateId - 5
                for (thickness in listOf("tip_merge", "tip", "frustum", "middle", "base")) {
                    for (verticalDirection in listOf("up", "down")) {
                        for (waterlogged in listOf("true", "false")) {
                            put(
                                stateId,
                                BlockState(
                                    default.block,
                                    stateId,
                                    linkedMapOf(
                                        "thickness" to thickness,
                                        "vertical_direction" to verticalDirection,
                                        "waterlogged" to waterlogged,
                                    ),
                                ),
                            )
                            stateId++
                        }
                    }
                }
            }

            // parity: reports/blocks.json state order used by NetherFortressPieces.
            val netherBrickFence =
                REGISTRY.byIdentifier(Identifier.vanilla("nether_brick_fence"))
                    ?: error("unknown block 'nether_brick_fence'")
            var fenceStateId = netherBrickFence.stateId - 31
            for (east in listOf("true", "false")) {
                for (north in listOf("true", "false")) {
                    for (south in listOf("true", "false")) {
                        for (waterlogged in listOf("true", "false")) {
                            for (west in listOf("true", "false")) {
                                put(
                                    fenceStateId,
                                    BlockState(
                                        netherBrickFence.block,
                                        fenceStateId,
                                        linkedMapOf(
                                            "east" to east,
                                            "north" to north,
                                            "south" to south,
                                            "waterlogged" to waterlogged,
                                            "west" to west,
                                        ),
                                    ),
                                )
                                fenceStateId++
                            }
                        }
                    }
                }
            }

            val netherBrickStairs =
                REGISTRY.byIdentifier(Identifier.vanilla("nether_brick_stairs"))
                    ?: error("unknown block 'nether_brick_stairs'")
            for ((facing, offset) in mapOf("north" to 0, "south" to 20, "west" to 40, "east" to 60)) {
                val stairsStateId = netherBrickStairs.stateId + offset
                put(
                    stairsStateId,
                    BlockState(
                        netherBrickStairs.block,
                        stairsStateId,
                        linkedMapOf(
                            "facing" to facing,
                            "half" to "bottom",
                            "shape" to "straight",
                            "waterlogged" to "false",
                        ),
                    ),
                )
            }

            // parity: reports/blocks.json states emitted by Nether ruined portal templates and
            // BlackstoneReplaceProcessor after mirror/rotation and BlockAgeProcessor.
            val polishedBlackstoneBrickStairs =
                REGISTRY.byIdentifier(Identifier.vanilla("polished_blackstone_brick_stairs"))
                    ?: error("unknown block 'polished_blackstone_brick_stairs'")
            val portalStairIds =
                mapOf(
                    "north" to mapOf("top" to 22253, "bottom" to 22263),
                    "south" to mapOf("top" to 22273, "bottom" to 22283),
                    "west" to mapOf("top" to 22293, "bottom" to 22303),
                    "east" to mapOf("top" to 22313, "bottom" to 22323),
                )
            for ((facing, halves) in portalStairIds) {
                for ((half, stateId) in halves) {
                    put(
                        stateId,
                        BlockState(
                            polishedBlackstoneBrickStairs.block,
                            stateId,
                            linkedMapOf(
                                "facing" to facing,
                                "half" to half,
                                "shape" to "straight",
                                "waterlogged" to "false",
                            ),
                        ),
                    )
                }
            }

            for (
            (path, states) in
            mapOf(
                "polished_blackstone_slab" to mapOf("top" to 22738, "bottom" to 22740, "double" to 22742),
                "polished_blackstone_brick_slab" to mapOf("top" to 22247, "bottom" to 22249, "double" to 22251),
            )
            ) {
                val slab = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
                for ((type, stateId) in states) {
                    put(
                        stateId,
                        BlockState(
                            slab.block,
                            stateId,
                            linkedMapOf("type" to type, "waterlogged" to "false"),
                        ),
                    )
                }
            }

            val chest = REGISTRY.byIdentifier(Identifier.vanilla("chest")) ?: error("unknown block 'chest'")
            for ((facing, stateId) in mapOf("north" to 3988, "south" to 3994, "west" to 4000, "east" to 4006)) {
                put(
                    stateId,
                    BlockState(
                        chest.block,
                        stateId,
                        linkedMapOf("type" to "single", "facing" to facing, "waterlogged" to "false"),
                    ),
                )
            }

            // parity: reports/blocks.json, including the flowing lava levels stored by giant
            // ruined portal templates rather than collapsing them to the source state.
            for (level in 0..15) {
                val stateId = LAVA.stateId + level
                put(stateId, BlockState(LAVA.block, stateId, mapOf("level" to level.toString())))
            }

            val vine = REGISTRY.byIdentifier(Identifier.vanilla("vine")) ?: error("unknown block 'vine'")
            for (stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID) {
                val enabledBits = VINE_DEFAULT_STATE_ID - stateId
                put(
                    stateId,
                    BlockState(
                        vine.block,
                        stateId,
                        linkedMapOf(
                            "east" to booleanProperty(enabledBits and 16 != 0),
                            "north" to booleanProperty(enabledBits and 8 != 0),
                            "south" to booleanProperty(enabledBits and 4 != 0),
                            "up" to booleanProperty(enabledBits and 2 != 0),
                            "west" to booleanProperty(enabledBits and 1 != 0),
                        ),
                    ),
                )
            }

            val glowLichen =
                REGISTRY.byIdentifier(Identifier.vanilla("glow_lichen")) ?: error("unknown block 'glow_lichen'")
            for (stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID) {
                val falseBits = stateId - GLOW_LICHEN_FIRST_STATE_ID
                put(
                    stateId,
                    BlockState(
                        glowLichen.block,
                        stateId,
                        linkedMapOf(
                            "down" to booleanProperty(falseBits and 64 == 0),
                            "east" to booleanProperty(falseBits and 32 == 0),
                            "north" to booleanProperty(falseBits and 16 == 0),
                            "south" to booleanProperty(falseBits and 8 == 0),
                            "up" to booleanProperty(falseBits and 4 == 0),
                            "waterlogged" to booleanProperty(falseBits and 2 == 0),
                            "west" to booleanProperty(falseBits and 1 == 0),
                        ),
                    ),
                )
            }

            val sculkVein =
                REGISTRY.byIdentifier(Identifier.vanilla("sculk_vein")) ?: error("unknown block 'sculk_vein'")
            for (stateId in sculkVein.stateId - 127..sculkVein.stateId) {
                val falseBits = stateId - (sculkVein.stateId - 127)
                put(
                    stateId,
                    BlockState(
                        sculkVein.block,
                        stateId,
                        linkedMapOf(
                            "down" to booleanProperty(falseBits and 64 == 0),
                            "east" to booleanProperty(falseBits and 32 == 0),
                            "north" to booleanProperty(falseBits and 16 == 0),
                            "south" to booleanProperty(falseBits and 8 == 0),
                            "up" to booleanProperty(falseBits and 4 == 0),
                            "waterlogged" to booleanProperty(falseBits and 2 == 0),
                            "west" to booleanProperty(falseBits and 1 == 0),
                        ),
                    ),
                )
            }

            val sculkSensor =
                REGISTRY.byIdentifier(Identifier.vanilla("sculk_sensor")) ?: error("unknown block 'sculk_sensor'")
            var sculkSensorStateId = sculkSensor.stateId - 1
            for (power in 0..15) {
                for (phase in listOf("inactive", "active", "cooldown")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            sculkSensorStateId,
                            BlockState(
                                sculkSensor.block,
                                sculkSensorStateId,
                                linkedMapOf(
                                    "power" to power.toString(),
                                    "sculk_sensor_phase" to phase,
                                    "waterlogged" to waterlogged,
                                ),
                            ),
                        )
                        sculkSensorStateId++
                    }
                }
            }

            val sculkCatalyst =
                REGISTRY.byIdentifier(Identifier.vanilla("sculk_catalyst")) ?: error("unknown block 'sculk_catalyst'")
            for ((bloom, stateId) in listOf("true", "false").zip(sculkCatalyst.stateId - 1..sculkCatalyst.stateId)) {
                put(stateId, BlockState(sculkCatalyst.block, stateId, mapOf("bloom" to bloom)))
            }

            val sculkShrieker =
                REGISTRY.byIdentifier(Identifier.vanilla("sculk_shrieker")) ?: error("unknown block 'sculk_shrieker'")
            var sculkShriekerStateId = sculkShrieker.stateId - 7
            for (canSummon in listOf("true", "false")) {
                for (shrieking in listOf("true", "false")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            sculkShriekerStateId,
                            BlockState(
                                sculkShrieker.block,
                                sculkShriekerStateId,
                                linkedMapOf(
                                    "can_summon" to canSummon,
                                    "shrieking" to shrieking,
                                    "waterlogged" to waterlogged,
                                ),
                            ),
                        )
                        sculkShriekerStateId++
                    }
                }
            }

            val sweetBerry =
                REGISTRY.byIdentifier(Identifier.vanilla("sweet_berry_bush"))
                    ?: error("unknown block 'sweet_berry_bush'")
            for (age in 0..3) {
                put(
                    sweetBerry.stateId + age,
                    BlockState(
                        sweetBerry.block,
                        sweetBerry.stateId + age,
                        mapOf(
                            "age" to age.toString(),
                        ),
                    ),
                )
            }

            val bamboo = REGISTRY.byIdentifier(Identifier.vanilla("bamboo")) ?: error("unknown block 'bamboo'")
            var bambooStateId = bamboo.stateId
            for (age in 0..1) {
                for (leaves in listOf("none", "small", "large")) {
                    for (stage in 0..1) {
                        put(
                            bambooStateId,
                            BlockState(
                                bamboo.block,
                                bambooStateId,
                                linkedMapOf(
                                    "age" to age.toString(),
                                    "leaves" to leaves,
                                    "stage" to stage.toString(),
                                ),
                            ),
                        )
                        bambooStateId++
                    }
                }
            }

            val cocoa = REGISTRY.byIdentifier(Identifier.vanilla("cocoa")) ?: error("unknown block 'cocoa'")
            val facings = listOf("north", "south", "west", "east")
            for (age in 0..2) {
                for ((facingOffset, facing) in facings.withIndex()) {
                    val stateId = cocoa.stateId + age * facings.size + facingOffset
                    put(
                        stateId,
                        BlockState(
                            cocoa.block,
                            stateId,
                            linkedMapOf(
                                "age" to age.toString(),
                                "facing" to facing,
                            ),
                        ),
                    )
                }
            }

            val paleHangingMoss =
                REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss"))
                    ?: error("unknown block 'pale_hanging_moss'")
            put(
                paleHangingMoss.stateId,
                BlockState(
                    paleHangingMoss.block,
                    paleHangingMoss.stateId,
                    mapOf(
                        "tip" to "true",
                    ),
                ),
            )
            put(
                paleHangingMoss.stateId + 1,
                BlockState(
                    paleHangingMoss.block,
                    paleHangingMoss.stateId + 1,
                    mapOf(
                        "tip" to "false",
                    ),
                ),
            )

            val creakingHeart =
                REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart")) ?: error("unknown block 'creaking_heart'")
            val axes = listOf("x", "y", "z")
            val heartStates = listOf("uprooted", "dormant", "awake")
            var stateId = CREAKING_HEART_FIRST_STATE_ID
            for (axis in axes) {
                for (heartState in heartStates) {
                    for (natural in listOf("true", "false")) {
                        put(
                            stateId,
                            BlockState(
                                creakingHeart.block,
                                stateId,
                                linkedMapOf("axis" to axis, "creaking_heart_state" to heartState, "natural" to natural),
                            ),
                        )
                        stateId++
                    }
                }
            }

            val mangrovePropagule =
                REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule"))
                    ?: error("unknown block 'mangrove_propagule'")
            var propaguleStateId = MANGROVE_PROPAGULE_FIRST_STATE_ID
            for (age in 0..4) {
                for (hanging in listOf("true", "false")) {
                    for (stage in 0..1) {
                        for (waterlogged in listOf("true", "false")) {
                            put(
                                propaguleStateId,
                                BlockState(
                                    mangrovePropagule.block,
                                    propaguleStateId,
                                    linkedMapOf(
                                        "age" to age.toString(),
                                        "hanging" to hanging,
                                        "stage" to stage.toString(),
                                        "waterlogged" to waterlogged,
                                    ),
                                ),
                            )
                            propaguleStateId++
                        }
                    }
                }
            }

            val leafLitter =
                REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter")) ?: error("unknown block 'leaf_litter'")
            var leafLitterStateId = LEAF_LITTER_DEFAULT_STATE_ID
            for (facing in listOf("north", "south", "west", "east")) {
                for (segmentAmount in 1..4) {
                    put(
                        leafLitterStateId,
                        BlockState(
                            leafLitter.block,
                            leafLitterStateId,
                            linkedMapOf("facing" to facing, "segment_amount" to segmentAmount.toString()),
                        ),
                    )
                    leafLitterStateId++
                }
            }

            val beeNest = REGISTRY.byIdentifier(Identifier.vanilla("bee_nest")) ?: error("unknown block 'bee_nest'")
            var beeNestStateId = BEE_NEST_DEFAULT_STATE_ID
            for (facing in listOf("north", "south", "west", "east")) {
                for (honeyLevel in 0..5) {
                    put(
                        beeNestStateId,
                        BlockState(
                            beeNest.block,
                            beeNestStateId,
                            linkedMapOf("facing" to facing, "honey_level" to honeyLevel.toString()),
                        ),
                    )
                    beeNestStateId++
                }
            }

            val chorusPlant =
                REGISTRY.byIdentifier(Identifier.vanilla("chorus_plant")) ?: error("unknown block 'chorus_plant'")
            for (stateId in CHORUS_PLANT_FIRST_STATE_ID..CHORUS_PLANT_DEFAULT_STATE_ID) {
                val falseBits = stateId - CHORUS_PLANT_FIRST_STATE_ID
                put(
                    stateId,
                    BlockState(
                        chorusPlant.block,
                        stateId,
                        linkedMapOf(
                            "down" to booleanProperty(falseBits and 32 == 0),
                            "east" to booleanProperty(falseBits and 16 == 0),
                            "north" to booleanProperty(falseBits and 8 == 0),
                            "south" to booleanProperty(falseBits and 4 == 0),
                            "up" to booleanProperty(falseBits and 2 == 0),
                            "west" to booleanProperty(falseBits and 1 == 0),
                        ),
                    ),
                )
            }

            val chorusFlower =
                REGISTRY.byIdentifier(Identifier.vanilla("chorus_flower")) ?: error("unknown block 'chorus_flower'")
            for (age in 0..5) {
                put(
                    CHORUS_FLOWER_DEFAULT_STATE_ID + age,
                    BlockState(
                        chorusFlower.block,
                        CHORUS_FLOWER_DEFAULT_STATE_ID + age,
                        mapOf("age" to age.toString()),
                    ),
                )
            }

            for (stateId in IRON_BARS.stateId - 31..IRON_BARS.stateId) {
                val falseBits = stateId - (IRON_BARS.stateId - 31)
                put(
                    stateId,
                    BlockState(
                        IRON_BARS.block,
                        stateId,
                        linkedMapOf(
                            "east" to booleanProperty(falseBits and 16 == 0),
                            "north" to booleanProperty(falseBits and 8 == 0),
                            "south" to booleanProperty(falseBits and 4 == 0),
                            "waterlogged" to booleanProperty(falseBits and 2 == 0),
                            "west" to booleanProperty(falseBits and 1 == 0),
                        ),
                    ),
                )
            }

            for (path in LEAF_BLOCKS) {
                val leaves = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
                var leafStateId = leaves.stateId - 27
                for (distance in 1..7) {
                    for (persistent in listOf("true", "false")) {
                        for (waterlogged in listOf("true", "false")) {
                            put(
                                leafStateId,
                                BlockState(
                                    leaves.block,
                                    leafStateId,
                                    linkedMapOf(
                                        "distance" to distance.toString(),
                                        "persistent" to persistent,
                                        "waterlogged" to waterlogged,
                                    ),
                                ),
                            )
                            leafStateId++
                        }
                    }
                }
            }

            for (path in DOUBLE_PLANT_BLOCKS) {
                val plant = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
                put(plant.stateId - 1, BlockState(plant.block, plant.stateId - 1, mapOf("half" to "upper")))
                put(plant.stateId, BlockState(plant.block, plant.stateId, mapOf("half" to "lower")))
            }

            for (defaultStateId in SNOWY_BLOCK_DEFAULT_STATE_IDS) {
                val block = REGISTRY.byId(defaultStateId)?.block ?: error("unknown snowy block state '$defaultStateId'")
                put(defaultStateId - 1, BlockState(block, defaultStateId - 1, mapOf("snowy" to "true")))
                put(defaultStateId, BlockState(block, defaultStateId, mapOf("snowy" to "false")))
            }

            for (defaultStateId in CORAL_WATERLOGGED_DEFAULT_STATE_IDS) {
                val block = REGISTRY.byId(defaultStateId)?.block ?: error("unknown coral state '$defaultStateId'")
                put(defaultStateId, BlockState(block, defaultStateId, mapOf("waterlogged" to "true")))
                put(defaultStateId + 1, BlockState(block, defaultStateId + 1, mapOf("waterlogged" to "false")))
            }

            for (defaultStateId in CORAL_WALL_FAN_DEFAULT_STATE_IDS) {
                val block = REGISTRY.byId(defaultStateId)?.block ?: error("unknown wall coral state '$defaultStateId'")
                var wallCoralStateId = defaultStateId
                for (facing in listOf("north", "south", "west", "east")) {
                    for (waterlogged in listOf("true", "false")) {
                        put(
                            wallCoralStateId,
                            BlockState(
                                block,
                                wallCoralStateId,
                                linkedMapOf("facing" to facing, "waterlogged" to waterlogged),
                            ),
                        )
                        wallCoralStateId++
                    }
                }
            }

            for (defaultStateId in HUGE_MUSHROOM_DEFAULT_STATE_IDS) {
                val block =
                    REGISTRY.byId(defaultStateId)?.block
                        ?: error("unknown huge mushroom state '$defaultStateId'")
                var mushroomStateId = defaultStateId
                for (down in listOf("true", "false")) {
                    for (east in listOf("true", "false")) {
                        for (north in listOf("true", "false")) {
                            for (south in listOf("true", "false")) {
                                for (up in listOf("true", "false")) {
                                    for (west in listOf("true", "false")) {
                                        put(
                                            mushroomStateId,
                                            BlockState(
                                                block,
                                                mushroomStateId,
                                                linkedMapOf(
                                                    "down" to down,
                                                    "east" to east,
                                                    "north" to north,
                                                    "south" to south,
                                                    "up" to up,
                                                    "west" to west,
                                                ),
                                            ),
                                        )
                                        mushroomStateId++
                                    }
                                }
                            }
                        }
                    }
                }
            }

            for (path in listOf("pink_petals", "wildflowers")) {
                val flowerBed = REGISTRY.byIdentifier(Identifier.vanilla(path)) ?: error("unknown block '$path'")
                var flowerBedStateId = flowerBed.stateId
                for (facing in listOf("north", "south", "west", "east")) {
                    for (flowerAmount in 1..4) {
                        put(
                            flowerBedStateId,
                            BlockState(
                                flowerBed.block,
                                flowerBedStateId,
                                linkedMapOf("facing" to facing, "flower_amount" to flowerAmount.toString()),
                            ),
                        )
                        flowerBedStateId++
                    }
                }
            }

            for (age in 0..25) {
                put(KELP.stateId + age, BlockState(KELP.block, KELP.stateId + age, mapOf("age" to age.toString())))
            }

            for (defaultStateId in GROWING_VINE_HEAD_DEFAULT_STATE_IDS) {
                val block = REGISTRY.byId(defaultStateId)?.block ?: error("unknown vine head '$defaultStateId'")
                for (age in 0..25) {
                    put(defaultStateId + age, BlockState(block, defaultStateId + age, mapOf("age" to age.toString())))
                }
            }

            var seaPickleStateId = SEA_PICKLE.stateId
            for (pickles in 1..4) {
                for (waterlogged in listOf("true", "false")) {
                    put(
                        seaPickleStateId,
                        BlockState(
                            SEA_PICKLE.block,
                            seaPickleStateId,
                            linkedMapOf("pickles" to pickles.toString(), "waterlogged" to waterlogged),
                        ),
                    )
                    seaPickleStateId++
                }
            }

            // parity: village templates need arbitrary palette properties and rotate them after
            // loading. These compact generated state-definition specs expand in the registry's
            // exact Cartesian-product order, covering every rotation result, not only states that
            // happened to appear unrotated in one NBT template.
            for (encoded in VILLAGE_BLOCK_STATE_SPECS + PORTAL_BLOCK_STATE_SPECS) {
                val fields = encoded.split('|', limit = 3)
                var stateId = fields[0].toInt()
                val block = Identifier.vanilla(fields[1])
                val propertySpecs =
                    fields[2]
                        .split(';')
                        .filter(String::isNotEmpty)
                        .map { property ->
                            val separator = property.indexOf('=')
                            property.substring(0, separator) to property.substring(separator + 1).split(',')
                        }
                val properties = linkedMapOf<String, String>()

                fun addStates(propertyIndex: Int) {
                    if (propertyIndex == propertySpecs.size) {
                        put(stateId, BlockState(block, stateId, properties.toMap()))
                        stateId++
                        return
                    }
                    val (name, values) = propertySpecs[propertyIndex]
                    for (value in values) {
                        properties[name] = value
                        addStates(propertyIndex + 1)
                    }
                    properties.remove(name)
                }
                addStates(0)
            }
        }
    }

    private val derivedStateIds: Map<StateKey, Int> by lazy {
        derivedStatesById.values.associate { StateKey(it.block, it.properties) to it.stateId }
    }

    private fun booleanProperty(value: Boolean): String = if (value) "true" else "false"

    private val HORIZONTAL_FACING_OFFSETS: Map<String, Int> =
        mapOf("north" to 0, "south" to 1, "west" to 2, "east" to 3)

    private val AXIS_BLOCKS: Set<String> =
        setOf(
            "oak_log",
            "spruce_log",
            "birch_log",
            "jungle_log",
            "acacia_log",
            "cherry_log",
            "dark_oak_log",
            "pale_oak_log",
            "mangrove_log",
        )
    private val AMETHYST_CLUSTER_BLOCKS: List<String> =
        listOf("amethyst_cluster", "large_amethyst_bud", "medium_amethyst_bud", "small_amethyst_bud")
    private val SPELEOTHEM_BLOCKS: List<String> = listOf("pointed_dripstone", "sulfur_spike")
}
