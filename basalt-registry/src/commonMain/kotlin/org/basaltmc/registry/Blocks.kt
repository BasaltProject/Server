package org.basaltmc.registry

/** A single block state, identified by its vanilla network state ID. */
public data class BlockState(
    public val block: Identifier,
    public val stateId: Int,
)

/**
 * Every block's default state, regenerated straight from the real 26.2 server jar's own data
 * generator output (see [ALL_DEFAULT_BLOCK_STATES]'s KDoc): authoritative state IDs, not
 * decompiled or guessed. Non-default states (block properties: facing, waterlogged, age...)
 * aren't modeled yet, since nothing in `basalt-world`/`basalt-entity` needs them yet either; every
 * block here is its default state only. A handful of commonly-referenced blocks get named
 * constants for readability at call sites; every other block is still in [REGISTRY], just without
 * its own named `val`.
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

    /** Whether this default block state exposes a non-empty water fluid state. */
    public fun hasWaterFluid(stateId: Int): Boolean =
        stateId in WATER_FLUID_STATE_IDS ||
            isGlowLichenState(stateId) && ((stateId - GLOW_LICHEN_FIRST_STATE_ID) and GLOW_LICHEN_WATERLOGGED_FALSE_BIT) == 0

    /** Whether [stateId] is one of glow lichen's 128 property combinations. */
    public fun isGlowLichenState(stateId: Int): Boolean =
        stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID

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
}
