package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * The `minecraft:block_entity_type` registry: which blocks own one, and the numeric id a chunk
 * packet carries for it.
 *
 * A block entity type is not always named after its block, and that is not a detail: all eight
 * copper chests are `minecraft:chest`, every shulker box is `minecraft:shulker_box`. Naming one
 * after its block left it without a protocol id, so it was dropped from the chunk and the block
 * turned invisible.
 */
public object BlockEntityTypes {
    private val idsByName: Map<String, Int> =
        ALL_BLOCK_ENTITY_TYPE_NAMES.withIndex().associate { (id, name) -> name to id }

    private val typesByBlock: Map<Identifier, Identifier> =
        ALL_BLOCK_ENTITY_OWNERS.associate { (block, type) ->
            Identifier.vanilla(block) to Identifier.vanilla(type)
        }

    /** The protocol ID for `minecraft:<path>`, or null for a type this version does not register. */
    public fun idOf(identifier: Identifier): Int? =
        if (identifier.namespace == Identifier.VANILLA_NAMESPACE) idsByName[identifier.path] else null

    /** The block entity [block] owns, or null when it owns none. parity: `BlockEntityType.isValid`. */
    public fun ofBlock(block: Identifier): Identifier? = typesByBlock[block]

    public val size: Int get() = ALL_BLOCK_ENTITY_TYPE_NAMES.size
}
