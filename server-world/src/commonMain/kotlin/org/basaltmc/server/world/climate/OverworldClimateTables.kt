package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder. The static parameter spans and
 * biome lookup tables `addBiomes` walks, plus the lookup helpers (`pick*`) that only read them.
 * Read-only after construction: nothing in [OverworldBiomeBuilder]'s call graph mutates a field.
 */
internal class OverworldClimateTables {
    internal val fullRange = Parameter.span(-1.0f, 1.0f)

    internal val temperatures =
        arrayOf(
            Parameter.span(-1.0f, -0.45f),
            Parameter.span(-0.45f, -0.15f),
            Parameter.span(-0.15f, 0.2f),
            Parameter.span(0.2f, 0.55f),
            Parameter.span(0.55f, 1.0f),
        )
    internal val humidities =
        arrayOf(
            Parameter.span(-1.0f, -0.35f),
            Parameter.span(-0.35f, -0.1f),
            Parameter.span(-0.1f, 0.1f),
            Parameter.span(0.1f, 0.3f),
            Parameter.span(0.3f, 1.0f),
        )
    internal val erosions =
        arrayOf(
            Parameter.span(-1.0f, -0.78f),
            Parameter.span(-0.78f, -0.375f),
            Parameter.span(-0.375f, -0.2225f),
            Parameter.span(-0.2225f, 0.05f),
            Parameter.span(0.05f, 0.45f),
            Parameter.span(0.45f, 0.55f),
            Parameter.span(0.55f, 1.0f),
        )

    internal val frozenRange = temperatures[0]
    internal val unfrozenRange = Parameter.span(temperatures[1], temperatures[4])

    internal val mushroomFieldsContinentalness = Parameter.span(-1.2f, -1.05f)
    internal val deepOceanContinentalness = Parameter.span(-1.05f, -0.455f)
    internal val oceanContinentalness = Parameter.span(-0.455f, -0.19f)
    internal val coastContinentalness = Parameter.span(-0.19f, -0.11f)
    internal val inlandContinentalness = Parameter.span(-0.11f, 0.55f)
    internal val nearInlandContinentalness = Parameter.span(-0.11f, 0.03f)
    internal val midInlandContinentalness = Parameter.span(0.03f, 0.3f)
    internal val farInlandContinentalness = Parameter.span(0.3f, 1.0f)

    internal val oceans =
        arrayOf(
            arrayOf(
                Biomes.DEEP_FROZEN_OCEAN,
                Biomes.DEEP_COLD_OCEAN,
                Biomes.DEEP_OCEAN,
                Biomes.DEEP_LUKEWARM_OCEAN,
                Biomes.WARM_OCEAN,
            ),
            arrayOf(Biomes.FROZEN_OCEAN, Biomes.COLD_OCEAN, Biomes.OCEAN, Biomes.LUKEWARM_OCEAN, Biomes.WARM_OCEAN),
        )
    internal val middleBiomes =
        arrayOf(
            arrayOf(Biomes.SNOWY_PLAINS, Biomes.SNOWY_PLAINS, Biomes.SNOWY_PLAINS, Biomes.SNOWY_TAIGA, Biomes.TAIGA),
            arrayOf(Biomes.PLAINS, Biomes.PLAINS, Biomes.FOREST, Biomes.TAIGA, Biomes.OLD_GROWTH_SPRUCE_TAIGA),
            arrayOf(Biomes.FLOWER_FOREST, Biomes.PLAINS, Biomes.FOREST, Biomes.BIRCH_FOREST, Biomes.DARK_FOREST),
            arrayOf(Biomes.SAVANNA, Biomes.SAVANNA, Biomes.FOREST, Biomes.JUNGLE, Biomes.JUNGLE),
            arrayOf(Biomes.DESERT, Biomes.DESERT, Biomes.DESERT, Biomes.DESERT, Biomes.DESERT),
        )
    internal val middleBiomesVariant =
        arrayOf(
            arrayOf(Biomes.ICE_SPIKES, null, Biomes.SNOWY_TAIGA, null, null),
            arrayOf(null, null, null, null, Biomes.OLD_GROWTH_PINE_TAIGA),
            arrayOf(Biomes.SUNFLOWER_PLAINS, null, null, Biomes.OLD_GROWTH_BIRCH_FOREST, null),
            arrayOf(null, null, Biomes.PLAINS, Biomes.SPARSE_JUNGLE, Biomes.BAMBOO_JUNGLE),
            arrayOf(null, null, null, null, null),
        )
    internal val plateauBiomes =
        arrayOf(
            arrayOf(
                Biomes.SNOWY_PLAINS,
                Biomes.SNOWY_PLAINS,
                Biomes.SNOWY_PLAINS,
                Biomes.SNOWY_TAIGA,
                Biomes.SNOWY_TAIGA,
            ),
            arrayOf(Biomes.MEADOW, Biomes.MEADOW, Biomes.FOREST, Biomes.TAIGA, Biomes.OLD_GROWTH_SPRUCE_TAIGA),
            arrayOf(Biomes.MEADOW, Biomes.MEADOW, Biomes.MEADOW, Biomes.MEADOW, Biomes.PALE_GARDEN),
            arrayOf(Biomes.SAVANNA_PLATEAU, Biomes.SAVANNA_PLATEAU, Biomes.FOREST, Biomes.FOREST, Biomes.JUNGLE),
            arrayOf(Biomes.BADLANDS, Biomes.BADLANDS, Biomes.BADLANDS, Biomes.WOODED_BADLANDS, Biomes.WOODED_BADLANDS),
        )
    internal val plateauBiomesVariant =
        arrayOf(
            arrayOf(Biomes.ICE_SPIKES, null, null, null, null),
            arrayOf(Biomes.CHERRY_GROVE, null, Biomes.MEADOW, Biomes.MEADOW, Biomes.OLD_GROWTH_PINE_TAIGA),
            arrayOf(Biomes.CHERRY_GROVE, Biomes.CHERRY_GROVE, Biomes.FOREST, Biomes.BIRCH_FOREST, null),
            arrayOf(null, null, null, null, null),
            arrayOf(Biomes.ERODED_BADLANDS, Biomes.ERODED_BADLANDS, null, null, null),
        )
    internal val shatteredBiomes =
        arrayOf(
            arrayOf(
                Biomes.WINDSWEPT_GRAVELLY_HILLS,
                Biomes.WINDSWEPT_GRAVELLY_HILLS,
                Biomes.WINDSWEPT_HILLS,
                Biomes.WINDSWEPT_FOREST,
                Biomes.WINDSWEPT_FOREST,
            ),
            arrayOf(
                Biomes.WINDSWEPT_GRAVELLY_HILLS,
                Biomes.WINDSWEPT_GRAVELLY_HILLS,
                Biomes.WINDSWEPT_HILLS,
                Biomes.WINDSWEPT_FOREST,
                Biomes.WINDSWEPT_FOREST,
            ),
            arrayOf(
                Biomes.WINDSWEPT_HILLS,
                Biomes.WINDSWEPT_HILLS,
                Biomes.WINDSWEPT_HILLS,
                Biomes.WINDSWEPT_FOREST,
                Biomes.WINDSWEPT_FOREST,
            ),
            arrayOf(null, null, null, null, null),
            arrayOf(null, null, null, null, null),
        )

    internal fun pickMiddleBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (weirdness.max < 0L) return middleBiomes[t][h]
        return middleBiomesVariant[t][h] ?: middleBiomes[t][h]
    }

    internal fun pickMiddleBiomeOrBadlandsIfHot(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = if (t == 4) pickBadlandsBiome(h, weirdness) else pickMiddleBiome(t, h, weirdness)

    internal fun pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = if (t == 0) pickSlopeBiome(t, h, weirdness) else pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)

    internal fun maybePickWindsweptSavannaBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
        underlyingBiome: Biome,
    ): Biome = if (t > 1 && h < 4 && weirdness.max >= 0L) Biomes.WINDSWEPT_SAVANNA else underlyingBiome

    internal fun pickShatteredCoastBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        val beachOrMiddleBiome = if (weirdness.max >= 0L) pickMiddleBiome(t, h, weirdness) else pickBeachBiome(t, h)
        return maybePickWindsweptSavannaBiome(t, h, weirdness, beachOrMiddleBiome)
    }

    internal fun pickBeachBiome(
        t: Int,
        h: Int,
    ): Biome {
        if (t == 0) return Biomes.SNOWY_BEACH
        return if (t == 4) Biomes.DESERT else Biomes.BEACH
    }

    internal fun pickBadlandsBiome(
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (h < 2) return if (weirdness.max < 0L) Biomes.BADLANDS else Biomes.ERODED_BADLANDS
        return if (h < 3) Biomes.BADLANDS else Biomes.WOODED_BADLANDS
    }

    internal fun pickPlateauBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (weirdness.max >= 0L) {
            plateauBiomesVariant[t][h]?.let { return it }
        }
        return plateauBiomes[t][h]
    }

    internal fun pickPeakBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (t <= 2) return if (weirdness.max < 0L) Biomes.JAGGED_PEAKS else Biomes.FROZEN_PEAKS
        return if (t == 3) Biomes.STONY_PEAKS else pickBadlandsBiome(h, weirdness)
    }

    internal fun pickSlopeBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (t >= 3) return pickPlateauBiome(t, h, weirdness)
        return if (h <= 1) Biomes.SNOWY_SLOPES else Biomes.GROVE
    }

    internal fun pickShatteredBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = shatteredBiomes[t][h] ?: pickMiddleBiome(t, h, weirdness)
}
