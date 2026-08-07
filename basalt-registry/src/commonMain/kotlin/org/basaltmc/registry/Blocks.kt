package org.basaltmc.registry

/** A single block state, identified by its vanilla network state ID. */
public data class BlockState(
    public val block: Identifier,
    public val stateId: Int,
)

/**
 * Hand-authored subset of the vanilla block registry: just enough for an in-memory flat world.
 * The full registry is [basalt-datagen]'s job once it exists.
 *
 * // parity: state IDs read from `minecraft-data`'s `26.1` data set (`blocks.json`), the closest
 * available reference to protocol 776; not yet cross-checked against the real 26.2 server jar.
 * These particular blocks (air, stone, dirt, bedrock, and grass_block's non-snowy default state)
 * have stayed at the same IDs across many past versions, but this is provisional until reconciled.
 */
public object Blocks {
    public val REGISTRY: Registry<BlockState> = Registry()

    public val AIR: BlockState = register(0, "air")
    public val STONE: BlockState = register(1, "stone")
    public val GRASS_BLOCK: BlockState = register(9, "grass_block")
    public val DIRT: BlockState = register(10, "dirt")
    public val BEDROCK: BlockState = register(85, "bedrock")

    private fun register(
        stateId: Int,
        path: String,
    ): BlockState {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(stateId, identifier, BlockState(identifier, stateId))
    }
}
