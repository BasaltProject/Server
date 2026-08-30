package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.terrain.RuinedPortalPlacement
import org.basaltmc.server.world.terrain.RuinedPortalSetup
import org.basaltmc.server.world.terrain.RuinedPortalVariant

/** The seven Overworld ruined portal structures. // parity: `worldgen/structure/ruined_portal*.json`. */
public object RuinedPortalVariants {
    public fun overworld(tags: BiomeTagLoader): List<RuinedPortalVariant> =
        listOf(
            variant(
                "ruined_portal",
                tags,
                "ruined_portal_standard",
                setup(
                    RuinedPortalPlacement.UNDERGROUND,
                    airPocket = 1.0f,
                    mossiness = 0.2f,
                    canBeCold = true,
                    weight = 0.5f,
                ),
                setup(
                    RuinedPortalPlacement.ON_LAND_SURFACE,
                    airPocket = 0.5f,
                    mossiness = 0.2f,
                    canBeCold = true,
                    weight = 0.5f,
                ),
            ),
            variant(
                "ruined_portal_desert",
                tags,
                "ruined_portal_desert",
                setup(RuinedPortalPlacement.PARTLY_BURIED, airPocket = 0.0f, mossiness = 0.0f),
            ),
            variant(
                "ruined_portal_jungle",
                tags,
                "ruined_portal_jungle",
                setup(
                    RuinedPortalPlacement.ON_LAND_SURFACE,
                    airPocket = 0.5f,
                    mossiness = 0.8f,
                    overgrown = true,
                    vines = true,
                ),
            ),
            variant(
                "ruined_portal_swamp",
                tags,
                "ruined_portal_swamp",
                setup(RuinedPortalPlacement.ON_OCEAN_FLOOR, airPocket = 0.0f, mossiness = 0.5f, vines = true),
            ),
            variant(
                "ruined_portal_mountain",
                tags,
                "ruined_portal_mountain",
                setup(
                    RuinedPortalPlacement.IN_MOUNTAIN,
                    airPocket = 1.0f,
                    mossiness = 0.2f,
                    canBeCold = true,
                    weight = 0.5f,
                ),
                setup(
                    RuinedPortalPlacement.ON_LAND_SURFACE,
                    airPocket = 0.5f,
                    mossiness = 0.2f,
                    canBeCold = true,
                    weight = 0.5f,
                ),
            ),
            variant(
                "ruined_portal_ocean",
                tags,
                "ruined_portal_ocean",
                setup(RuinedPortalPlacement.ON_OCEAN_FLOOR, airPocket = 0.0f, mossiness = 0.8f, canBeCold = true),
            ),
            variant(
                "ruined_portal_nether",
                tags,
                "ruined_portal_nether",
                setup(
                    RuinedPortalPlacement.IN_NETHER,
                    airPocket = 0.5f,
                    mossiness = 0.0f,
                    replaceWithBlackstone = true,
                ),
            ),
        )

    private fun variant(
        name: String,
        tags: BiomeTagLoader,
        tag: String,
        vararg setups: RuinedPortalSetup,
    ) = RuinedPortalVariant(
        name,
        tags.biomesIn("minecraft:has_structure/$tag"),
        setups.toList(),
    )

    private fun setup(
        placement: RuinedPortalPlacement,
        airPocket: Float,
        mossiness: Float,
        overgrown: Boolean = false,
        vines: Boolean = false,
        canBeCold: Boolean = false,
        replaceWithBlackstone: Boolean = false,
        weight: Float = 1.0f,
    ) = RuinedPortalSetup(placement, airPocket, mossiness, overgrown, vines, canBeCold, replaceWithBlackstone, weight)
}
