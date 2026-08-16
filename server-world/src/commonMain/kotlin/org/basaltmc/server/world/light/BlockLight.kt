package org.basaltmc.server.world.light

import org.basaltmc.server.registry.Blocks
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
    public fun emission(stateId: Int): Int = if (stateId in 0 until TABLE.size) TABLE[stateId] else 0

    /**
     * The block light of the middle chunk of [window], or null when nothing in reach burns.
     *
     * The band is the sections that hold something burning, grown by one on each side: fifteen
     * levels reach less than a section away, so nothing outside it can be lit.
     */
    public fun compute(window: LightWindow): Array<IntArray>? {
        val sections = window.emitterSections(::emission) ?: return null
        val bottomY = (window.minY + (sections.first - 1) * 16).coerceAtLeast(window.minY)
        val topY = (window.minY + (sections.last + 2) * 16 - 1).coerceAtMost(window.maxY)
        val pass = LightSpread(window, bottomY, topY, outside = 0)
        for (z in 0 until LightWindow.WIDTH) {
            for (x in 0 until LightWindow.WIDTH) {
                for (y in bottomY..topY) {
                    val emitted = emission(pass.stateAt(x, y, z))
                    if (emitted > 0) pass.seed(x, y, z, emitted)
                }
            }
        }
        return pass.run()
    }

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

    /** The same table [SkyLight] keeps, and for the same reason: this is read per block. */
    private val TABLE: IntArray = IntArray(Blocks.stateCount) { EMISSION[it] ?: 0 }
}
