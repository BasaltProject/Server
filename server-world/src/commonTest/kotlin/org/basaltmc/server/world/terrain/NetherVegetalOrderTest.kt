package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.terrain.decoration.nether.NetherVegetalDecoration
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The two huge fungi were once crossed here, invisible in a chunk but seeding both from the wrong
 * stream. The order is the one `FeatureSorter` derives from the five Nether biomes: warped_forest
 * sorts before crimson_forest even though crimson_forest is numbered first.
 */
class NetherVegetalOrderTest {
    @Test
    fun `the nether vegetal step keeps the order its seeds are derived from`() {
        assertEquals(
            listOf(
                "spring_lava",
                "brown_mushroom_normal",
                "red_mushroom_normal",
                "warped_fungi",
                "warped_forest_vegetation",
                "nether_sprouts",
                "twisting_vines",
                "weeping_vines",
                "crimson_fungi",
                "crimson_forest_vegetation",
            ),
            NetherVegetalDecoration.FEATURE_ORDER,
        )
    }
}
