package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * The `minecraft:data_component_type` registry: the numbers an item stack's component patch carries
 * and the names `/give diamond_sword[custom_name=...]` writes.
 *
 * Ids only. What each component *means* is the codec's business, and this server writes a codec for
 * the ones a command can set; the rest still resolve by name, which is what lets an unsupported one
 * be refused by name instead of by number.
 */
public object DataComponents {
    private val byName: Map<Identifier, Int> =
        ALL_DATA_COMPONENT_IDS.associate { (id, path) -> Identifier.vanilla(path) to id }

    private val byId: Map<Int, Identifier> = byName.entries.associate { (name, id) -> id to name }

    public val all: Set<Identifier> get() = byName.keys

    public operator fun get(name: Identifier): Int? = byName[name]

    public fun nameOf(id: Int): Identifier? = byId[id]
}
