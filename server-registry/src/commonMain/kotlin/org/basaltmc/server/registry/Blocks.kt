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

    /** // parity: BlockState.isAir, including the two non-default air blocks used by worldgen. */
    public fun isAir(stateId: Int): Boolean =
        stateId == AIR.stateId || stateId == VOID_AIR.stateId || stateId == CAVE_AIR.stateId

    private fun register(
        stateId: Int,
        path: String,
    ): BlockState {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(stateId, identifier, BlockState(identifier, stateId))
    }
}
