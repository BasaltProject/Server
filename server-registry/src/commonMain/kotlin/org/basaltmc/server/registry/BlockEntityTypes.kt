package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * // parity: the `minecraft:block_entity_type` registry, whose numeric IDs are the order
 * `BlockEntityTypeIds` declares its keys in. Protocol facts, like the block and item tables beside
 * this one: the network ID of a block entity is what a chunk packet carries for it.
 */
public object BlockEntityTypes {
    /** In registry order, so an index into this list is the protocol ID. */
    private val NAMES: List<String> =
        listOf(
            "furnace",
            "chest",
            "trapped_chest",
            "ender_chest",
            "jukebox",
            "dispenser",
            "dropper",
            "sign",
            "hanging_sign",
            "mob_spawner",
            "creaking_heart",
            "piston",
            "brewing_stand",
            "enchanting_table",
            "end_portal",
            "beacon",
            "skull",
            "daylight_detector",
            "hopper",
            "comparator",
            "banner",
            "structure_block",
            "end_gateway",
            "command_block",
            "shulker_box",
            "conduit",
            "barrel",
            "smoker",
            "blast_furnace",
            "lectern",
            "bell",
            "jigsaw",
            "campfire",
            "beehive",
            "sculk_sensor",
            "calibrated_sculk_sensor",
            "sculk_catalyst",
            "sculk_shrieker",
            "chiseled_bookshelf",
            "shelf",
            "brushable_block",
            "decorated_pot",
            "crafter",
            "trial_spawner",
            "vault",
            "test_block",
            "test_instance_block",
            "copper_golem_statue",
            "potent_sulfur",
        )

    private val idsByName: Map<String, Int> = NAMES.withIndex().associate { (id, name) -> name to id }

    /** The protocol ID for `minecraft:<path>`, or null for a type this version does not register. */
    public fun idOf(identifier: Identifier): Int? =
        if (identifier.namespace == Identifier.VANILLA_NAMESPACE) idsByName[identifier.path] else null

    public val size: Int get() = NAMES.size
}
