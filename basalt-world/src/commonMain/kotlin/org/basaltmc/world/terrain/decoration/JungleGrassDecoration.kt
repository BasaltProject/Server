package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Real `patch_grass_jungle` (`BiomeDefaultFeatures.addJungleGrass`), ported from the real jar's own
 * `VegetationPlacements`/`VegetationFeatures`. Block choice is `WeightedStateProvider` real weights
 * `short_grass:3, fern:1`: `nextInt(4) < 3` picks short grass, the same cumulative-weight reduction
 * already used for [DefaultVegetationDecoration]'s own flower choice. The real "not on podzol"
 * predicate is not ported (this project's `AirTargetFilter` only checks air, see its own KDoc).
 */
public object JungleGrassDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ r, _ -> if (r.nextInt(4) < 3) shortGrassStateId else fernStateId })
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in
            listOf(
                CountPlacement(25),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(32),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> feature.place(chunk, random, pos) }
    }

    private val shortGrassStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_grass"))?.stateId ?: error("unknown block 'short_grass'")
    private val fernStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("fern"))?.stateId ?: error("unknown block 'fern'")
}
