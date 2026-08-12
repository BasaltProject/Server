package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/** Minecraft 26.2 `dry_grass` weighted simple-block feature and its two placements. */
public object DryGrassDecoration {
    public val DESERT: BiomeVegetalDecoration = placed(rarity = 3)
    public val BADLANDS: BiomeVegetalDecoration = placed(rarity = 6)

    private fun placed(rarity: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val feature =
                SimpleBlockFeature({ providerRandom, _ ->
                    if (providerRandom.nextInt(2) == 0) shortDryGrassStateId else tallDryGrassStateId
                })
            runPipeline(
                chunk,
                random,
                listOf(
                    RarityFilter(rarity),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(64),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
                feature,
            )
        }

    private fun runPipeline(
        chunk: Chunk,
        random: WorldgenRandom,
        modifiers: List<PlacementModifier>,
        feature: SimpleBlockFeature,
    ) {
        val context = PlacementContext(chunk)
        var positions: Sequence<DecorationPos> = sequenceOf(DecorationPos(chunk.chunkX * 16, 0, chunk.chunkZ * 16))
        for (modifier in modifiers) positions = positions.flatMap { modifier.getPositions(context, random, it) }
        positions.forEach { position -> feature.place(chunk, random, position) }
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val shortDryGrassStateId = stateId("short_dry_grass")
    private val tallDryGrassStateId = stateId("tall_dry_grass")
}
