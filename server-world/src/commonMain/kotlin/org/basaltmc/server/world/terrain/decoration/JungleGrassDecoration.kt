package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * Real `patch_grass_jungle` (`BiomeDefaultFeatures.addJungleGrass`), ported from the real jar's own
 * `VegetationPlacements`/`VegetationFeatures`. Block choice is `WeightedStateProvider` real weights
 * `short_grass:3, fern:1`: `nextInt(4) < 3` picks short grass, the same cumulative-weight reduction
 * already used for [DefaultVegetationDecoration]'s own flower choice.
 */
public object JungleGrassDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (
        modifier in
        listOf(
            CountPlacement(25),
            InSquarePlacement,
            HeightmapPlacement,
        )
        ) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { origin -> placePatchAt(chunk, random, origin, context) }
    }

    internal fun placePatchAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        context: PlacementContext = PlacementContext(chunk),
    ) {
        val feature = SimpleBlockFeature({ r, _ -> if (r.nextInt(4) < 3) shortGrassStateId else fernStateId })
        var positions: Sequence<DecorationPos> = sequenceOf(origin)
        for (modifier in listOf(
            CountPlacement(32),
            RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
            AIR_NOT_ON_PODZOL,
        )) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { position -> feature.place(chunk, random, position) }
    }

    private val shortGrassStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("short_grass"))?.stateId ?: error("unknown block 'short_grass'")
    private val fernStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("fern"))?.stateId ?: error("unknown block 'fern'")
    private val AIR_NOT_ON_PODZOL =
        PlacementFilter { context, _, position ->
            if (position.y <= org.basaltmc.server.world.WorldHeight.MIN_Y ||
                position.y > org.basaltmc.server.world.WorldHeight.MAX_Y
            ) {
                false
            } else {
                val target = context.chunk.getDecorationBlockStateId(position.x, position.y, position.z)
                val below = context.chunk.getDecorationBlockStateId(position.x, position.y - 1, position.z)
                Blocks.isAir(target) && Blocks.byStateId(below)?.block?.path != "podzol"
            }
        }
}
