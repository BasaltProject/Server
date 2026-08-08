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
    public fun hasWaterFluid(stateId: Int): Boolean =
        stateId in WATER_FLUID_STATE_IDS ||
            isGlowLichenState(stateId) && ((stateId - GLOW_LICHEN_FIRST_STATE_ID) and GLOW_LICHEN_WATERLOGGED_FALSE_BIT) == 0

    /** Whether [stateId] is one of glow lichen's 128 property combinations. */
    public fun isGlowLichenState(stateId: Int): Boolean =
        stateId in GLOW_LICHEN_FIRST_STATE_ID..GLOW_LICHEN_DEFAULT_STATE_ID

    /** Whether [stateId] is one of vine's 32 face-property combinations. */
    public fun isVineState(stateId: Int): Boolean =
        stateId in VINE_FIRST_STATE_ID..VINE_DEFAULT_STATE_ID

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
        }
    }

    private val derivedStateIds: Map<StateKey, Int> by lazy {
        derivedStatesById.values.associate { StateKey(it.block, it.properties) to it.stateId }
    }

    private fun booleanProperty(value: Boolean): String = if (value) "true" else "false"

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
