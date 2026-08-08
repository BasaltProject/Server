package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk

/** Taiga large ferns and the two taiga-specific grass/fern ground-cover variants. */
public object TaigaGroundDecoration {
    /** `patch_berry_rare`, appended after extra vegetation in snowy taiga. */
    public val RARE_BERRY: BiomeVegetalDecoration = berry(chance = 384)

    /** `patch_berry_common`, appended after extra vegetation in the other three taigas. */
    public val COMMON_BERRY: BiomeVegetalDecoration = berry(chance = 32)

    /** `patch_large_fern`, placed before trees in all four taiga variants. */
    public val LARGE_FERN: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            place(
                chunk,
                random,
                listOf(
                    RarityFilter(5),
                    InSquarePlacement,
                    HeightmapPlacement,
                    CountPlacement(96),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(alsoCheckAbove = true),
                ),
                SimpleBlockFeature({ _, _ -> largeFernStateId }, isDoublePlant = true),
            )
        }

    /** `patch_grass_taiga_2`, one outer attempt in taiga and snowy taiga. */
    public val NORMAL: BiomeVegetalDecoration = grass(outerCount = 1)

    /** Seven `patch_grass_taiga` attempts followed by `patch_dead_bush`. */
    public val OLD_GROWTH: BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            grass(outerCount = 7).decorate(chunk, random)
            place(
                chunk,
                random,
                listOf(
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                    CountPlacement(4),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                ),
                SimpleBlockFeature({ _, _ -> deadBushStateId }),
            )
        }

    private fun grass(outerCount: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            val modifiers = ArrayList<PlacementModifier>()
            if (outerCount != 1) modifiers += CountPlacement(outerCount)
            modifiers += InSquarePlacement
            modifiers += HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE)
            modifiers += CountPlacement(32)
            modifiers += RandomOffsetPlacement(xzSpread = 7, ySpread = 3)
            modifiers += AirTargetFilter()
            place(
                chunk,
                random,
                modifiers,
                SimpleBlockFeature({ providerRandom, _ ->
                    if (providerRandom.nextInt(5) == 0) shortGrassStateId else fernStateId
                }),
            )
        }

    private fun berry(chance: Int): BiomeVegetalDecoration =
        BiomeVegetalDecoration { chunk, random ->
            place(
                chunk,
                random,
                listOf(
                    RarityFilter(chance),
                    InSquarePlacement,
                    HeightmapPlacement(Chunk.HeightmapType.WORLD_SURFACE),
                    CountPlacement(96),
                    RandomOffsetPlacement(xzSpread = 7, ySpread = 3),
                    AirTargetFilter(),
                    GrassBelowFilter,
                ),
                SimpleBlockFeature({ _, _ -> sweetBerryAge3StateId }),
            )
        }

    private fun place(
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

    private val largeFernStateId = stateId("large_fern")
    private val shortGrassStateId = stateId("short_grass")
    private val fernStateId = stateId("fern")
    private val deadBushStateId = stateId("dead_bush")

    // The 26.2 state definition orders age=0..3 contiguously. The generated default-state table
    // identifies age=0; the configured berry_bush feature explicitly requests age=3.
    private val sweetBerryAge3StateId = stateId("sweet_berry_bush") + 3

    private object GrassBelowFilter : PlacementFilter {
        override fun shouldPlace(
            context: PlacementContext,
            random: WorldgenRandom,
            origin: DecorationPos,
        ): Boolean =
            context.chunk.getBlockStateId(origin.x and 15, origin.y - 1, origin.z and 15) == Blocks.GRASS_BLOCK.stateId
    }
}
