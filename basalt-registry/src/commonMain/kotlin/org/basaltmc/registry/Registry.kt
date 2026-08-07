package org.basaltmc.registry

/**
 * Two-way mapping between an [Identifier], a numeric protocol/network ID, and a value, matching
 * vanilla's own registry model (`net.minecraft.core.Registry`). IDs are assigned explicitly at
 * [register] time rather than auto-incremented: vanilla's own numbering (especially block states)
 * is not simple insertion order, so callers must supply the real ID.
 */
public class Registry<T> {
    private val valueById = LinkedHashMap<Int, T>()
    private val idByIdentifier = LinkedHashMap<Identifier, Int>()
    private val identifierById = LinkedHashMap<Int, Identifier>()

    public fun register(
        id: Int,
        identifier: Identifier,
        value: T,
    ): T {
        require(id !in valueById) { "Duplicate registry id $id (already ${identifierById[id]})" }
        require(identifier !in idByIdentifier) { "Duplicate registry identifier $identifier" }
        valueById[id] = value
        idByIdentifier[identifier] = id
        identifierById[id] = identifier
        return value
    }

    public fun byId(id: Int): T? = valueById[id]

    public fun byIdentifier(identifier: Identifier): T? = idByIdentifier[identifier]?.let(valueById::get)

    public fun idOf(identifier: Identifier): Int? = idByIdentifier[identifier]

    public fun identifierOf(id: Int): Identifier? = identifierById[id]

    public val size: Int get() = valueById.size

    public fun values(): Collection<T> = valueById.values
}
