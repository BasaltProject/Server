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
    public val BEDROCK: BlockState = register(85, "bedrock")
    public val OAK_LOG: BlockState = register(137, "oak_log")
    public val OAK_LEAVES: BlockState = register(279, "oak_leaves")

    init {
        for ((stateId, path) in ALL_DEFAULT_BLOCK_STATES) {
            if (REGISTRY.byId(stateId) == null) register(stateId, path)
        }
    }

    private fun register(
        stateId: Int,
        path: String,
    ): BlockState {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(stateId, identifier, BlockState(identifier, stateId))
    }
}
