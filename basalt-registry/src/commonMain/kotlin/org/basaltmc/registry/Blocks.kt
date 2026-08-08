package org.basaltmc.registry

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
    public val FIREFLY_BUSH: BlockState = register(32365, "firefly_bush")

    init {
        for ((stateId, path) in ALL_DEFAULT_BLOCK_STATES) {
            if (REGISTRY.byId(stateId) == null) register(stateId, path)
        }
    }

    /** Resolves both registered default states and the non-default states emitted by worldgen. */
    public fun byStateId(stateId: Int): BlockState? = derivedStatesById[stateId] ?: REGISTRY.byId(stateId)

    /** Resolves a block plus its serialized vanilla properties back to a network state ID. */
    public fun stateIdOf(
        block: Identifier,
        properties: Map<String, String>,
    ): Int? {
        if (properties.isEmpty()) return REGISTRY.idOf(block)
        return derivedStateIds[StateKey(block, properties)]
    }

    /** Number of IDs required by vanilla's direct block-state palette. */
    public val stateCount: Int
        get() = maxOf(ALL_DEFAULT_BLOCK_STATES.last().first, derivedStatesById.keys.maxOrNull() ?: 0) + 1

    /** Whether this default block state exposes a non-empty water fluid state. */
    public fun hasWaterFluid(stateId: Int): Boolean {
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
        return wallCoralStateStart != null && (stateId - wallCoralStateStart) and 1 == 0
    }

    /** Whether [stateId] is one of the 28 distance, persistence, and water states of a leaves block. */
    public fun isLeavesState(stateId: Int): Boolean =
        stateId in LEAVES_FIRST_STATE_ID..LEAVES_LAST_STATE_ID

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
            SNOWY_BLOCK_DEFAULT_STATE_IDS.firstOrNull { candidate -> stateId in candidate - 1..candidate } ?: return null
        return if (snowy) defaultStateId - 1 else defaultStateId
    }

    /** Whether [stateId] is one of kelp's 26 age states. */
    public fun isKelpState(stateId: Int): Boolean = stateId in KELP.stateId..KELP.stateId + 25

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
    public fun isVineState(stateId: Int): Boolean =
        stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID

    /** Whether [stateId] is one of cocoa's age and facing combinations. */
    public fun isCocoaState(stateId: Int): Boolean =
        stateId in COCOA_DEFAULT_STATE_ID..COCOA_LAST_STATE_ID

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
    public fun isBeeNestState(stateId: Int): Boolean =
        stateId in BEE_NEST_DEFAULT_STATE_ID..BEE_NEST_LAST_STATE_ID

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
        REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss"))?.stateId ?: error("unknown block 'pale_hanging_moss'")
    private val PALE_HANGING_MOSS_LAST_STATE_ID: Int = PALE_HANGING_MOSS_DEFAULT_STATE_ID + 1
    private val CREAKING_HEART_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart"))?.stateId ?: error("unknown block 'creaking_heart'")
    private val CREAKING_HEART_FIRST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID - 7
    private val CREAKING_HEART_LAST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID + 10
    private val MANGROVE_PROPAGULE_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule"))?.stateId ?: error("unknown block 'mangrove_propagule'")
    private val MANGROVE_PROPAGULE_FIRST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID - 5
    private val MANGROVE_PROPAGULE_LAST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID + 34
    private val LEAF_LITTER_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter"))?.stateId ?: error("unknown block 'leaf_litter'")
    private val LEAF_LITTER_LAST_STATE_ID: Int = LEAF_LITTER_DEFAULT_STATE_ID + 15
    private val BEE_NEST_DEFAULT_STATE_ID: Int =
        REGISTRY.byIdentifier(Identifier.vanilla("bee_nest"))?.stateId ?: error("unknown block 'bee_nest'")
    private val BEE_NEST_LAST_STATE_ID: Int = BEE_NEST_DEFAULT_STATE_ID + 23
    private val DOUBLE_PLANT_BLOCKS: List<String> =
        listOf("sunflower", "lilac", "rose_bush", "peony", "tall_grass", "large_fern", "tall_seagrass")
    private val DOUBLE_PLANT_DEFAULT_STATE_IDS: IntArray =
        DOUBLE_PLANT_BLOCKS.map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val SNOWY_BLOCK_DEFAULT_STATE_IDS: IntArray =
        listOf("grass_block", "podzol", "mycelium").map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val CORAL_WATERLOGGED_DEFAULT_STATE_IDS: IntArray =
        listOf(
            "tube_coral", "brain_coral", "bubble_coral", "fire_coral", "horn_coral",
            "tube_coral_fan", "brain_coral_fan", "bubble_coral_fan", "fire_coral_fan", "horn_coral_fan",
        ).map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val CORAL_WALL_FAN_DEFAULT_STATE_IDS: IntArray =
        listOf(
            "tube_coral_wall_fan", "brain_coral_wall_fan", "bubble_coral_wall_fan",
            "fire_coral_wall_fan", "horn_coral_wall_fan",
        ).map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val HUGE_MUSHROOM_DEFAULT_STATE_IDS: IntArray =
        listOf("brown_mushroom_block", "red_mushroom_block", "mushroom_stem").map { path ->
            REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
    private val TALL_SEAGRASS_UPPER_STATE_ID: Int = TALL_SEAGRASS.stateId - 1
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

            val glowLichen = REGISTRY.byIdentifier(Identifier.vanilla("glow_lichen")) ?: error("unknown block 'glow_lichen'")
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

            val sweetBerry = REGISTRY.byIdentifier(Identifier.vanilla("sweet_berry_bush")) ?: error("unknown block 'sweet_berry_bush'")
            for (age in 0..3) {
                put(sweetBerry.stateId + age, BlockState(sweetBerry.block, sweetBerry.stateId + age, mapOf("age" to age.toString())))
            }

            val cocoa = REGISTRY.byIdentifier(Identifier.vanilla("cocoa")) ?: error("unknown block 'cocoa'")
            val facings = listOf("north", "south", "west", "east")
            for (age in 0..2) {
                for ((facingOffset, facing) in facings.withIndex()) {
                    val stateId = cocoa.stateId + age * facings.size + facingOffset
                    put(stateId, BlockState(cocoa.block, stateId, linkedMapOf("age" to age.toString(), "facing" to facing)))
                }
            }

            val paleHangingMoss =
                REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss")) ?: error("unknown block 'pale_hanging_moss'")
            put(paleHangingMoss.stateId, BlockState(paleHangingMoss.block, paleHangingMoss.stateId, mapOf("tip" to "true")))
            put(paleHangingMoss.stateId + 1, BlockState(paleHangingMoss.block, paleHangingMoss.stateId + 1, mapOf("tip" to "false")))

            val creakingHeart = REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart")) ?: error("unknown block 'creaking_heart'")
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
                REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule")) ?: error("unknown block 'mangrove_propagule'")
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

            val leafLitter = REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter")) ?: error("unknown block 'leaf_litter'")
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
}
