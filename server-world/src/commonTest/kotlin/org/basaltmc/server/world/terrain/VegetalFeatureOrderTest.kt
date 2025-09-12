package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.terrain.decoration.OverworldVegetalDecoration
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The index of a vegetal feature is its seed, so reordering one silently regenerates every feature
 * after it. The order came from running vanilla's own `FeatureSorter` over the Overworld biome list,
 * which needs Mojang data and cannot run here: this pins the shape and the indices others hardcode.
 */
class VegetalFeatureOrderTest {
    private val order = OverworldVegetalDecoration.featureOrder()

    @Test
    fun `the step runs its features as a gapless run from zero`() {
        assertEquals(FEATURE_COUNT, order.size, "the Overworld's vegetal step has $FEATURE_COUNT features")
        assertEquals(
            List(FEATURE_COUNT) { it },
            order.map { it.first },
            "indices must run 0 until $FEATURE_COUNT with no gap",
        )
        assertEquals(order.size, order.map { it.second }.toSet().size, "two features cannot share a name")
    }

    @Test
    fun `the indices other files seed themselves with still name the same feature`() {
        // Every index hardcoded outside OverworldVegetalDecoration, with the file that holds it.
        val anchors =
            mapOf(
                0 to "glow_lichen", // GlowLichenDecoration
                2 to "bamboo", // BambooDecoration
                6 to "bamboo_light", // BambooDecoration
                8 to "patch_tall_grass", // TallGrassDecoration
                14 to "pale_moss_patch", // PaleMossDecoration
                26 to "patch_tall_grass_2", // TallGrassDecoration
                51 to "patch_bush", // BushPatchDecoration
                65 to "patch_waterlily", // WaterlilyPatchDecoration
                77 to "patch_leaf_litter", // LeafLitterPatchDecoration
                79 to "mushroom_island_vegetation", // MushroomFieldsDecoration, MushroomPatchDecoration
                82 to "patch_pumpkin", // PumpkinPatchDecoration
                91 to "vines", // VinePatchDecoration
                96 to "warm_ocean_vegetation", // CoralDecoration
            )
        val byIndex = order.toMap()
        for ((index, name) in anchors) {
            assertEquals(name, byIndex[index], "index $index is seeded by name elsewhere in the tree")
        }
    }

    private companion object {
        const val FEATURE_COUNT = 106
    }
}
