package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * The shared `flowerDefault` patch behind `flower_default` (index 57) and `flower_warm` (index 10),
 * which differ only in their rarity filter. // parity: WeightedStateProvider weights poppy:2,
 * dandelion:1, so `nextInt(3) < 2` picks poppy.
 */
public object DefaultVegetationDecoration {
    /** The vanilla placed feature names of the flower patches this draws. */
    public val PLACED_IDS: Set<String> = setOf("flower_default", "flower_warm")

    /** The patch at one origin, which is what `/place feature` runs. */
    public fun placeDefaultPatchAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val feature = SimpleBlockFeature({ r, _ -> if (r.nextInt(3) < 2) poppyStateId else dandelionStateId })
        placeFrom(
            chunk,
            random,
            origin,
            listOf(CountPlacement(64), RandomOffsetPlacement(xzSpread = 7, ySpread = 3), AirTargetFilter()),
        ) { position -> feature.place(chunk, random, position) }
    }

    private val poppyStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("poppy"))?.stateId ?: error("unknown block 'poppy'")
    private val dandelionStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dandelion"))?.stateId ?: error("unknown block 'dandelion'")
}
