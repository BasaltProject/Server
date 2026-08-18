package org.basaltmc.server.registry

/**
 * What each item leaves in the grid when it is crafted with. An item absent from here is
 * used up, which is nearly all of them.
 *
 * Not reported by the data generator, so it comes from a harness run against the real
 * classes (see `basalt.datagen`).
 */
internal val ALL_CRAFTING_REMAINDERS: List<Pair<String, String>> =
    listOf(
        "dragon_breath" to "glass_bottle",
        "honey_bottle" to "glass_bottle",
        "lava_bucket" to "bucket",
        "milk_bucket" to "bucket",
        "water_bucket" to "bucket",
    )
