package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * Light from the blocks themselves: lava, glowstone, a torch in a cave.
 *
 * parity: `BlockLightEngine`, seeded from each block's own light level instead of from the sky.
 * The spread is the same one [SkyLight] uses, so the two stay in step.
 *
 * The emission table is written here rather than generated: the official `blocks.json` report does
 * not carry light levels, and a block that emits none needs no entry.
 */
public object BlockLight {
    /** parity: `BlockBehaviour.Properties.lightLevel` of the blocks that have one. */
    public fun emission(stateId: Int): Int = EMISSION[stateId] ?: 0

    /** Whether this chunk has anything worth lighting, so an empty one costs nothing. */
    public fun hasEmitters(chunk: Chunk): Boolean =
        (0 until chunk.sectionCount).any { index ->
            chunk.section(index).mayContain { emission(it) > 0 }
        }

    public fun compute(
        chunk: Chunk,
        blockAt: (x: Int, y: Int, z: Int) -> Int?,
    ): Array<IntArray> = LightSpread.compute(chunk, blockAt, ::emission)

    private val EMISSION: Map<Int, Int> =
        buildMap {
            fun put(
                level: Int,
                vararg names: String,
            ) {
                for (name in names) {
                    val id = Blocks.REGISTRY.idOf(Identifier.vanilla(name)) ?: continue
                    put(id, level)
                }
            }
            put(
                15,
                "beacon",
                "conduit",
                "end_gateway",
                "end_portal",
                "fire",
                "glowstone",
                "jack_o_lantern",
                "lantern",
                "lava",
                "lava_cauldron",
                "sea_lantern",
                "shroomlight",
                "campfire",
                "ochre_froglight",
                "pearlescent_froglight",
                "verdant_froglight",
                "respawn_anchor",
            )
            put(14, "torch", "wall_torch", "end_rod")
            put(13, "amethyst_cluster")
            put(11, "nether_portal", "crying_obsidian")
            put(10, "soul_torch", "soul_wall_torch", "soul_lantern", "soul_fire", "soul_campfire")
            put(9, "deepslate_redstone_ore", "redstone_ore")
            put(7, "enchanting_table", "ender_chest", "glow_lichen", "redstone_torch", "large_amethyst_bud")
            put(6, "sculk_catalyst", "medium_amethyst_bud")
            put(4, "small_amethyst_bud")
            put(3, "magma_block", "sculk_sensor", "sculk_shrieker")
            put(1, "brewing_stand", "brown_mushroom", "dragon_egg", "sculk_vein")
        }
}
