package org.basaltmc.server.registry

/**
 * Which trim material each item provides. An item absent from this list cannot trim armour.
 *
 * Read from the item component report the data generator writes (see `basalt.datagen`).
 */
internal val ALL_TRIM_MATERIALS: List<Pair<String, String>> =
    listOf(
        "amethyst_shard" to "amethyst",
        "copper_ingot" to "copper",
        "diamond" to "diamond",
        "emerald" to "emerald",
        "gold_ingot" to "gold",
        "iron_ingot" to "iron",
        "lapis_lazuli" to "lapis",
        "netherite_ingot" to "netherite",
        "quartz" to "quartz",
        "redstone" to "redstone",
        "resin_brick" to "resin",
    )
