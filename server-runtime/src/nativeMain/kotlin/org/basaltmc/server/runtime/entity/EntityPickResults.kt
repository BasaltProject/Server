package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.Items
import org.basaltmc.wire.common.Identifier

/**
 * What middle-clicking an entity puts in your hand.
 *
 * parity: `Entity.getPickResult` and its overrides: a mob answers with its own spawn egg, the
 * things that were placed from an item answer with that item, and everything else answers with
 * nothing. It is a list rather than "the item with the same name" on purpose: an arrow, a snowball
 * and a lit tnt all share a name with an item and none of them can be picked.
 */
public object EntityPickResults {
    public fun itemFor(type: Identifier): Identifier? {
        if (type.namespace != Identifier.VANILLA_NAMESPACE) return null
        val path = type.path
        // parity: `LeashFenceKnotEntity.getPickResult`, the one whose item is not called after it.
        if (path == LEASH_KNOT) return itemOrNull(LEAD)
        itemOrNull(path + SPAWN_EGG_SUFFIX)?.let { return it }
        if (path in PLACED || path.endsWith(BOAT_SUFFIX) || path.endsWith(RAFT_SUFFIX)) return itemOrNull(path)
        return null
    }

    private fun itemOrNull(path: String): Identifier? =
        Identifier.vanilla(path).takeIf { Items.REGISTRY.byIdentifier(it) != null }

    /** parity: the `getPickResult` overrides that are not a spawn egg. */
    private val PLACED: Set<String> =
        setOf(
            "minecart",
            "chest_minecart",
            "furnace_minecart",
            "tnt_minecart",
            "hopper_minecart",
            "spawner_minecart",
            "command_block_minecart",
            "armor_stand",
            "painting",
            "item_frame",
            "glow_item_frame",
            "end_crystal",
        )

    private const val SPAWN_EGG_SUFFIX = "_spawn_egg"
    private const val BOAT_SUFFIX = "_boat"
    private const val RAFT_SUFFIX = "_raft"
    private const val LEASH_KNOT = "leash_knot"
    private const val LEAD = "lead"
}
