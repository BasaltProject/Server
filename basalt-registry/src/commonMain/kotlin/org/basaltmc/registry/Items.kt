package org.basaltmc.registry

/** A single item, identified by its vanilla network protocol ID. */
public data class Item(
    public val id: Identifier,
    public val protocolId: Int,
)

/**
 * Every item, regenerated straight from the real 26.2 server jar's own data generator output (see
 * [ALL_ITEMS]'s KDoc): authoritative protocol IDs, not decompiled or guessed.
 */
public object Items {
    public val REGISTRY: Registry<Item> = Registry()

    init {
        for ((protocolId, path) in ALL_ITEMS) {
            val identifier = Identifier.vanilla(path)
            REGISTRY.register(protocolId, identifier, Item(identifier, protocolId))
        }
    }
}
