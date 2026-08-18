package org.basaltmc.server.registry

/**
 * The blocks that move an entity differently from every other block: ice is slippery, soul
 * sand is slow, honey kills a jump and a bed bounces. Anything absent is the default 0.6
 * friction, with no change to speed or jump and no bounce at all.
 *
 * Not reported by the data generator, so it comes from a harness run against the real
 * classes (see `basalt.datagen`).
 */
internal val ALL_BLOCK_MOVEMENT: List<BlockMovementEntry> =
    listOf(
        BlockMovementEntry("black_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("blue_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("blue_ice", 0.989f, 1.0f, 1.0f, 0.0f),
        BlockMovementEntry("brown_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("cyan_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("frosted_ice", 0.98f, 1.0f, 1.0f, 0.0f),
        BlockMovementEntry("gray_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("green_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("honey_block", 0.6f, 0.4f, 0.5f, 0.0f),
        BlockMovementEntry("ice", 0.98f, 1.0f, 1.0f, 0.0f),
        BlockMovementEntry("light_blue_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("light_gray_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("lime_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("magenta_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("orange_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("packed_ice", 0.98f, 1.0f, 1.0f, 0.0f),
        BlockMovementEntry("pink_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("purple_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("red_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("slime_block", 0.8f, 1.0f, 1.0f, 1.0f),
        BlockMovementEntry("soul_sand", 0.6f, 0.4f, 1.0f, 0.0f),
        BlockMovementEntry("white_bed", 0.6f, 1.0f, 1.0f, 0.75f),
        BlockMovementEntry("yellow_bed", 0.6f, 1.0f, 1.0f, 0.75f),
    )
