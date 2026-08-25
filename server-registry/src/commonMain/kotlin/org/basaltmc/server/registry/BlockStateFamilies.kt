package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** Whether this default block state exposes a non-empty water fluid state. */
public fun Blocks.hasWaterFluid(stateId: Int): Boolean {
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
    return stateId in WATERLOGGED_STATE_TABLE.indices && WATERLOGGED_STATE_TABLE[stateId]
}

/** Whether this state exposes any non-empty fluid state. */
public fun Blocks.hasFluid(stateId: Int): Boolean = stateId in LAVA.stateId..LAVA.stateId + 15 || hasWaterFluid(stateId)

/** Whether [stateId] is one of the 28 distance, persistence, and water states of a leaves block. */
public fun Blocks.isLeavesState(stateId: Int): Boolean = stateId in LEAVES_FIRST_STATE_ID..LEAVES_LAST_STATE_ID

/** Returns the vanilla distance property for a leaves state, or null for any other block. */
public fun Blocks.leafDistance(stateId: Int): Int? =
    if (isLeavesState(stateId)) ((stateId - leafStateRangeStart(stateId)) / 4) + 1 else null

/** Changes only the distance property of a leaves state. */
public fun Blocks.withLeafDistance(
    stateId: Int,
    distance: Int,
): Int? {
    require(distance in 1..7)
    if (!isLeavesState(stateId)) return null
    return leafStateRangeStart(stateId) + (distance - 1) * 4 + (stateId and 3)
}

/** Whether [stateId] is an upper or lower half of a worldgen double plant. */
public fun Blocks.isDoublePlantState(stateId: Int): Boolean =
    DOUBLE_PLANT_DEFAULT_STATE_IDS.any { defaultStateId -> stateId in defaultStateId - 1..defaultStateId }

/** Returns the upper-half state matching a lower double-plant state. */
public fun Blocks.upperDoublePlantStateId(lowerStateId: Int): Int? =
    if (lowerStateId in DOUBLE_PLANT_DEFAULT_STATE_IDS) lowerStateId - 1 else null

/** Returns the matching snowy or bare state of grass block, podzol, or mycelium. */
public fun Blocks.withSnowy(
    stateId: Int,
    snowy: Boolean,
): Int? {
    val defaultStateId =
        SNOWY_BLOCK_DEFAULT_STATE_IDS.firstOrNull { candidate -> stateId in candidate - 1..candidate }
            ?: return null
    return if (snowy) defaultStateId - 1 else defaultStateId
}

/** Whether [stateId] is one of kelp's 26 age states. */
public fun Blocks.isKelpState(stateId: Int): Boolean = stateId in KELP.stateId..KELP.stateId + 25

/** Returns a twisting/weeping vine head state with vanilla's requested age. */
public fun Blocks.growingVineHeadStateId(
    stateId: Int,
    age: Int,
): Int? {
    require(age in 0..25)
    val defaultStateId = GROWING_VINE_HEAD_DEFAULT_STATE_IDS.firstOrNull { stateId in it..it + 25 } ?: return null
    return defaultStateId + age
}

/** Whether [stateId] is one of sea pickle's count and water states. */
public fun Blocks.isSeaPickleState(stateId: Int): Boolean = stateId in SEA_PICKLE.stateId..SEA_PICKLE.stateId + 7

/** Whether [stateId] is a live coral plant, floor fan, or wall fan state. */
public fun Blocks.isCoralPlantState(stateId: Int): Boolean =
    CORAL_WATERLOGGED_DEFAULT_STATE_IDS.any { stateId in it..it + 1 } ||
        CORAL_WALL_FAN_DEFAULT_STATE_IDS.any { stateId in it..it + 7 }

/** Returns a wall coral fan state with the requested horizontal facing and water state. */
public fun Blocks.coralWallFanStateId(
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
public fun Blocks.isHugeMushroomState(stateId: Int): Boolean =
    HUGE_MUSHROOM_DEFAULT_STATE_IDS.any { stateId in it..it + 63 }

/** Returns a huge mushroom block state with the requested visible faces. */
public fun Blocks.hugeMushroomStateId(
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
public fun Blocks.isWildflowersState(stateId: Int): Boolean =
    stateId in WILDFLOWERS_DEFAULT_STATE_ID..WILDFLOWERS_DEFAULT_STATE_ID + 15

/** Whether [stateId] is one of pink petals' 16 facing and amount states. */
public fun Blocks.isPinkPetalsState(stateId: Int): Boolean =
    stateId in PINK_PETALS_DEFAULT_STATE_ID..PINK_PETALS_DEFAULT_STATE_ID + 15

/** Whether [stateId] is one of glow lichen's 128 property combinations. */
public fun Blocks.isGlowLichenState(stateId: Int): Boolean =
    stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID

/** Whether [stateId] is one of vine's 32 face-property combinations. */
public fun Blocks.isVineState(stateId: Int): Boolean = stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID

/** Whether [stateId] is one of cocoa's age and facing combinations. */
public fun Blocks.isCocoaState(stateId: Int): Boolean = stateId in COCOA_DEFAULT_STATE_ID..COCOA_LAST_STATE_ID

/** Whether [stateId] is one of pale hanging moss's tip states. */
public fun Blocks.isPaleHangingMossState(stateId: Int): Boolean =
    stateId in PALE_HANGING_MOSS_DEFAULT_STATE_ID..PALE_HANGING_MOSS_LAST_STATE_ID

/** Whether [stateId] is one of creaking heart's axis, lifecycle, and natural combinations. */
public fun Blocks.isCreakingHeartState(stateId: Int): Boolean =
    stateId in CREAKING_HEART_FIRST_STATE_ID..CREAKING_HEART_LAST_STATE_ID

/** Whether [stateId] is one of mangrove propagule's age, hanging, stage, and waterlogged combinations. */
public fun Blocks.isMangrovePropaguleState(stateId: Int): Boolean =
    stateId in MANGROVE_PROPAGULE_FIRST_STATE_ID..MANGROVE_PROPAGULE_LAST_STATE_ID

/** Whether [stateId] is one of leaf litter's facing and segment amount combinations. */
public fun Blocks.isLeafLitterState(stateId: Int): Boolean =
    stateId in LEAF_LITTER_DEFAULT_STATE_ID..LEAF_LITTER_LAST_STATE_ID

/** Whether [stateId] is one of bee nest's facing and honey-level combinations. */
public fun Blocks.isBeeNestState(stateId: Int): Boolean = stateId in BEE_NEST_DEFAULT_STATE_ID..BEE_NEST_LAST_STATE_ID

/** Returns a chorus plant state with the six exact vanilla connection properties. */
public fun Blocks.chorusPlantStateId(
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
public fun Blocks.chorusFlowerStateId(age: Int): Int {
    require(age in 0..5)
    return CHORUS_FLOWER_DEFAULT_STATE_ID + age
}

/** Returns a dry iron-bars state with the exact vanilla horizontal connections. */
public fun Blocks.ironBarsStateId(
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

private fun leafStateRangeStart(stateId: Int): Int =
    LEAVES_FIRST_STATE_ID + ((stateId - LEAVES_FIRST_STATE_ID) / 28) * 28

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
    ).mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()

internal val GLOW_LICHEN_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("glow_lichen"))?.stateId ?: error("unknown block 'glow_lichen'")
internal val GLOW_LICHEN_FIRST_STATE_ID: Int = GLOW_LICHEN_DEFAULT_STATE_ID - 127
private const val GLOW_LICHEN_WATERLOGGED_FALSE_BIT: Int = 2
internal val VINE_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("vine"))?.stateId ?: error("unknown block 'vine'")
internal val VINE_FIRST_STATE_ID: Int = VINE_DEFAULT_STATE_ID - 31
private val COCOA_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("cocoa"))?.stateId ?: error("unknown block 'cocoa'")
private val COCOA_LAST_STATE_ID: Int = COCOA_DEFAULT_STATE_ID + 11
private val PALE_HANGING_MOSS_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("pale_hanging_moss"))?.stateId
        ?: error("unknown block 'pale_hanging_moss'")
private val PALE_HANGING_MOSS_LAST_STATE_ID: Int = PALE_HANGING_MOSS_DEFAULT_STATE_ID + 1
private val CREAKING_HEART_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("creaking_heart"))?.stateId
        ?: error("unknown block 'creaking_heart'")
internal val CREAKING_HEART_FIRST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID - 7
private val CREAKING_HEART_LAST_STATE_ID: Int = CREAKING_HEART_DEFAULT_STATE_ID + 10
private val MANGROVE_PROPAGULE_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mangrove_propagule"))?.stateId
        ?: error("unknown block 'mangrove_propagule'")
internal val MANGROVE_PROPAGULE_FIRST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID - 5
private val MANGROVE_PROPAGULE_LAST_STATE_ID: Int = MANGROVE_PROPAGULE_DEFAULT_STATE_ID + 34
internal val LEAF_LITTER_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("leaf_litter"))?.stateId ?: error("unknown block 'leaf_litter'")
private val LEAF_LITTER_LAST_STATE_ID: Int = LEAF_LITTER_DEFAULT_STATE_ID + 15
internal val BEE_NEST_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("bee_nest"))?.stateId ?: error("unknown block 'bee_nest'")
private val BEE_NEST_LAST_STATE_ID: Int = BEE_NEST_DEFAULT_STATE_ID + 23
internal val CHORUS_PLANT_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chorus_plant"))?.stateId ?: error("unknown block 'chorus_plant'")
internal val CHORUS_FLOWER_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("chorus_flower"))?.stateId
        ?: error("unknown block 'chorus_flower'")
internal val DOUBLE_PLANT_BLOCKS: List<String> =
    listOf("sunflower", "lilac", "rose_bush", "peony", "tall_grass", "large_fern", "tall_seagrass")
private val DOUBLE_PLANT_DEFAULT_STATE_IDS: IntArray =
    DOUBLE_PLANT_BLOCKS
        .map { path ->
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
internal val SNOWY_BLOCK_DEFAULT_STATE_IDS: IntArray =
    listOf("grass_block", "podzol", "mycelium")
        .map { path ->
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
internal val CORAL_WATERLOGGED_DEFAULT_STATE_IDS: IntArray =
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
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
    }.toIntArray()
internal val CORAL_WALL_FAN_DEFAULT_STATE_IDS: IntArray =
    listOf(
        "tube_coral_wall_fan",
        "brain_coral_wall_fan",
        "bubble_coral_wall_fan",
        "fire_coral_wall_fan",
        "horn_coral_wall_fan",
    ).map { path ->
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
    }.toIntArray()
internal val HUGE_MUSHROOM_DEFAULT_STATE_IDS: IntArray =
    listOf("brown_mushroom_block", "red_mushroom_block", "mushroom_stem")
        .map { path ->
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
private val TALL_SEAGRASS_UPPER_STATE_ID: Int = Blocks.TALL_SEAGRASS.stateId - 1
internal val GROWING_VINE_HEAD_DEFAULT_STATE_IDS: IntArray =
    listOf("weeping_vines", "twisting_vines")
        .map { path ->
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
        }.toIntArray()
private val WILDFLOWERS_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("wildflowers"))?.stateId ?: error("unknown block 'wildflowers'")
private val PINK_PETALS_DEFAULT_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("pink_petals"))?.stateId ?: error("unknown block 'pink_petals'")
private val LEAVES_FIRST_STATE_ID: Int = Blocks.OAK_LEAVES.stateId - 27
private val LEAVES_LAST_STATE_ID: Int =
    Blocks.REGISTRY.byIdentifier(Identifier.vanilla("flowering_azalea_leaves"))?.stateId
        ?: error("unknown block 'flowering_azalea_leaves'")

/** Every ordinary waterlogged state, decoded once from the authoritative generated definitions. */
private val WATERLOGGED_STATE_TABLE: BooleanArray by lazy(LazyThreadSafetyMode.PUBLICATION) {
    BooleanArray(Blocks.stateCount) { stateId ->
        BlockStateDefinitions.propertiesOf(stateId)["waterlogged"] == "true"
    }
}

private val HORIZONTAL_FACING_OFFSETS: Map<String, Int> =
    mapOf("north" to 0, "south" to 1, "west" to 2, "east" to 3)
