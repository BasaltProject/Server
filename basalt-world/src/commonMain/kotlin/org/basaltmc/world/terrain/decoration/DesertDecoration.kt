package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/**
 * Desert ground cover before [ExtraVegetationDecoration.DESERT]: `patch_dry_grass_desert`, then
 * `patch_dead_bush_2`.
 */
public object DesertDecoration : BiomeVegetalDecoration {
    override fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        DryGrassDecoration.DESERT.decorate(chunk, random)
        placeDeadBush(chunk, random)
    }

    // parity: placed_feature/patch_dead_bush_2.json (VegetationPlacements.worldSurfaceSquaredWithCount(2) + copyAndAdd).
    private fun placeDeadBush(
        chunk: Chunk,
        random: WorldgenRandom,
    ) {
        val feature = SimpleBlockFeature({ _, _ -> deadBushStateId })
        runPipeline(
            chunk,
            random,
            listOf(
                CountPlacement(2),
                InSquarePlacement,
                HeightmapPlacement,
                CountPlacement(4),
                RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                AirTargetFilter(),
            ),
        ) { c, r, pos -> feature.place(c, r, pos) }
    }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        place: (Chunk, WorldgenRandom, DecorationPos) -> Boolean,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) {
            positions = positions.flatMap { modifier.getPositions(context, random, it) }
        }
        positions.forEach { pos -> place(chunk, random, pos) }
    }

    private val deadBushStateId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("dead_bush"))?.stateId ?: error("unknown block 'dead_bush'")
}
