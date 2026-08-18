package org.basaltmc.server.registry

/**
 * The items whose block is not called the same as they are. Anything absent puts down the
 * block of its own name, which is nearly all of them.
 *
 * Not reported by the data generator, so it comes from a harness run against the real
 * classes (see `basalt.datagen`).
 */
internal val ALL_ITEM_BLOCKS: List<Pair<String, String>> =
    listOf(
        "beetroot_seeds" to "beetroots",
        "carrot" to "carrots",
        "cocoa_beans" to "cocoa",
        "glow_berries" to "cave_vines",
        "melon_seeds" to "melon_stem",
        "pitcher_pod" to "pitcher_crop",
        "potato" to "potatoes",
        "powder_snow_bucket" to "powder_snow",
        "pumpkin_seeds" to "pumpkin_stem",
        "redstone" to "redstone_wire",
        "string" to "tripwire",
        "sweet_berries" to "sweet_berry_bush",
        "torchflower_seeds" to "torchflower_crop",
        "wheat_seeds" to "wheat",
    )
