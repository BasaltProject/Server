package org.basaltmc.registry

/** A single biome, identified by its vanilla network ID. */
public data class Biome(
    public val id: Identifier,
)

/**
 * Hand-authored subset of the vanilla biome registry: just enough for an in-memory flat world.
 *
 * // parity: ID read from `minecraft-data`'s `26.1` data set (`biomes.json`); see [Blocks] for why
 * that is the closest currently available reference to protocol 776.
 */
public object Biomes {
    public val REGISTRY: Registry<Biome> = Registry()

    public val PLAINS: Biome = register(40, "plains")

    private fun register(
        id: Int,
        path: String,
    ): Biome {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(id, identifier, Biome(identifier))
    }
}
