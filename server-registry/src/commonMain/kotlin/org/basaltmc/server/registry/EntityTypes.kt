package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One entity type: its id and the protocol number the client resolves it by. */
public data class EntityTypeEntry(
    public val id: Identifier,
    public val protocolId: Int,
)

/**
 * The `minecraft:entity_type` registry, from [ALL_ENTITY_TYPE_IDS].
 *
 * Types only: what an entity *does* is the entity's own business, and this server has behaviour for
 * almost none of them. What it needs from the registry is the number to spawn one with, which is
 * what `/summon` looks up.
 */
public object EntityTypes {
    private val byName: Map<Identifier, EntityTypeEntry> =
        ALL_ENTITY_TYPE_IDS.associate { (protocolId, path) ->
            val id = Identifier.vanilla(path)
            id to EntityTypeEntry(id, protocolId)
        }

    private val byProtocolId: Map<Int, EntityTypeEntry> = byName.values.associateBy { it.protocolId }

    public val all: List<EntityTypeEntry> get() = byName.values.sortedBy { it.protocolId }

    public operator fun get(id: Identifier): EntityTypeEntry? = byName[id]

    public fun byProtocolId(protocolId: Int): EntityTypeEntry? = byProtocolId[protocolId]

    /** parity: `minecraft:player` cannot be summoned, and vanilla refuses it by name. */
    public val PLAYER: Identifier = Identifier.vanilla("player")
}
