package org.basaltmc.server.world.climate

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes

/**
 * // parity: net.minecraft.world.level.biome.OverworldBiomeBuilder, ported mechanically: every
 * threshold, table and branch below is vanilla's, not an approximation.
 *
 * Biomes are regions in the six-axis climate space rather than hand-placed borders, and
 * [ParameterList.findValue] picks the closest one to the sampled point, which is where vanilla's
 * soft curved borders come from. The debug-only members are not ported: they feed `F3` overlays
 * and a debug world type this project has not got.
 */
public class OverworldBiomeBuilder {
    private val fullRange = Parameter.span(-1.0f, 1.0f)

    private val temperatures =
        arrayOf(
            Parameter.span(-1.0f, -0.45f),
            Parameter.span(-0.45f, -0.15f),
            Parameter.span(-0.15f, 0.2f),
            Parameter.span(0.2f, 0.55f),
            Parameter.span(0.55f, 1.0f),
        )
    private val humidities =
        arrayOf(
            Parameter.span(-1.0f, -0.35f),
            Parameter.span(-0.35f, -0.1f),
            Parameter.span(-0.1f, 0.1f),
            Parameter.span(0.1f, 0.3f),
            Parameter.span(0.3f, 1.0f),
        )
    private val erosions =
        arrayOf(
            Parameter.span(-1.0f, -0.78f),
            Parameter.span(-0.78f, -0.375f),
            Parameter.span(-0.375f, -0.2225f),
            Parameter.span(-0.2225f, 0.05f),
            Parameter.span(0.05f, 0.45f),
            Parameter.span(0.45f, 0.55f),
            Parameter.span(0.55f, 1.0f),
        )

    private val frozenRange = temperatures[0]
    private val unfrozenRange = Parameter.span(temperatures[1], temperatures[4])

    private val mushroomFieldsContinentalness = Parameter.span(-1.2f, -1.05f)
    private val deepOceanContinentalness = Parameter.span(-1.05f, -0.455f)
    private val oceanContinentalness = Parameter.span(-0.455f, -0.19f)
    private val coastContinentalness = Parameter.span(-0.19f, -0.11f)
    private val inlandContinentalness = Parameter.span(-0.11f, 0.55f)
    private val nearInlandContinentalness = Parameter.span(-0.11f, 0.03f)
    private val midInlandContinentalness = Parameter.span(0.03f, 0.3f)
    private val farInlandContinentalness = Parameter.span(0.3f, 1.0f)

    private val oceans =
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
    private val middleBiomes =
        arrayOf(
            arrayOf(Biomes.SNOWY_PLAINS, Biomes.SNOWY_PLAINS, Biomes.SNOWY_PLAINS, Biomes.SNOWY_TAIGA, Biomes.TAIGA),
            arrayOf(Biomes.PLAINS, Biomes.PLAINS, Biomes.FOREST, Biomes.TAIGA, Biomes.OLD_GROWTH_SPRUCE_TAIGA),
            arrayOf(Biomes.FLOWER_FOREST, Biomes.PLAINS, Biomes.FOREST, Biomes.BIRCH_FOREST, Biomes.DARK_FOREST),
            arrayOf(Biomes.SAVANNA, Biomes.SAVANNA, Biomes.FOREST, Biomes.JUNGLE, Biomes.JUNGLE),
            arrayOf(Biomes.DESERT, Biomes.DESERT, Biomes.DESERT, Biomes.DESERT, Biomes.DESERT),
        )
    private val middleBiomesVariant =
        arrayOf(
            arrayOf(Biomes.ICE_SPIKES, null, Biomes.SNOWY_TAIGA, null, null),
            arrayOf(null, null, null, null, Biomes.OLD_GROWTH_PINE_TAIGA),
            arrayOf(Biomes.SUNFLOWER_PLAINS, null, null, Biomes.OLD_GROWTH_BIRCH_FOREST, null),
            arrayOf(null, null, Biomes.PLAINS, Biomes.SPARSE_JUNGLE, Biomes.BAMBOO_JUNGLE),
            arrayOf(null, null, null, null, null),
        )
    private val plateauBiomes =
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
    private val plateauBiomesVariant =
        arrayOf(
            arrayOf(Biomes.ICE_SPIKES, null, null, null, null),
            arrayOf(Biomes.CHERRY_GROVE, null, Biomes.MEADOW, Biomes.MEADOW, Biomes.OLD_GROWTH_PINE_TAIGA),
            arrayOf(Biomes.CHERRY_GROVE, Biomes.CHERRY_GROVE, Biomes.FOREST, Biomes.BIRCH_FOREST, null),
            arrayOf(null, null, null, null, null),
            arrayOf(Biomes.ERODED_BADLANDS, Biomes.ERODED_BADLANDS, null, null, null),
        )
    private val shatteredBiomes =
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

    /** // parity: OverworldBiomeBuilder.addBiomes. [SharedConstants.debugGenerateSquareTerrainWithoutNoise] is never true in this project: `addDebugBiomes` is not ported. */
    public fun addBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        addOffCoastBiomes(emit)
        addInlandBiomes(emit)
        addUndergroundBiomes(emit)
    }

    public fun build(): ParameterList<Biome> {
        val values = mutableListOf<Pair<ParameterPoint, Biome>>()
        addBiomes { point, biome -> values += point to biome }
        return ParameterList(values)
    }

    private fun addOffCoastBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        addSurfaceBiome(
            emit,
            fullRange,
            fullRange,
            mushroomFieldsContinentalness,
            fullRange,
            fullRange,
            0.0f,
            Biomes.MUSHROOM_FIELDS,
        )
        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            addSurfaceBiome(
                emit,
                temperature,
                fullRange,
                deepOceanContinentalness,
                fullRange,
                fullRange,
                0.0f,
                oceans[0][t],
            )
            addSurfaceBiome(
                emit,
                temperature,
                fullRange,
                oceanContinentalness,
                fullRange,
                fullRange,
                0.0f,
                oceans[1][t],
            )
        }
    }

    private fun addInlandBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        addMidSlice(emit, Parameter.span(-1.0f, -0.93333334f))
        addHighSlice(emit, Parameter.span(-0.93333334f, -0.7666667f))
        addPeaks(emit, Parameter.span(-0.7666667f, -0.56666666f))
        addHighSlice(emit, Parameter.span(-0.56666666f, -0.4f))
        addMidSlice(emit, Parameter.span(-0.4f, -0.26666668f))
        addLowSlice(emit, Parameter.span(-0.26666668f, -0.05f))
        addValleys(emit, Parameter.span(-0.05f, 0.05f))
        addLowSlice(emit, Parameter.span(0.05f, 0.26666668f))
        addMidSlice(emit, Parameter.span(0.26666668f, 0.4f))
        addHighSlice(emit, Parameter.span(0.4f, 0.56666666f))
        addPeaks(emit, Parameter.span(0.56666666f, 0.7666667f))
        addHighSlice(emit, Parameter.span(0.7666667f, 0.93333334f))
        addMidSlice(emit, Parameter.span(0.93333334f, 1.0f))
    }

    private fun addPeaks(
        emit: (ParameterPoint, Biome) -> Unit,
        weirdness: Parameter,
    ) {
        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            for (h in humidities.indices) {
                val humidity = humidities[h]
                val middleBiome = pickMiddleBiome(t, h, weirdness)
                val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
                val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                    pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
                val plateauBiome = pickPlateauBiome(t, h, weirdness)
                val shatteredBiome = pickShatteredBiome(t, h, weirdness)
                val shatteredBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, shatteredBiome)
                val peakBiome = pickPeakBiome(t, h, weirdness)
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[0],
                    weirdness,
                    0.0f,
                    peakBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    erosions[1],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHotOrSlopeIfCold,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[1],
                    weirdness,
                    0.0f,
                    peakBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    Parameter.span(erosions[2], erosions[3]),
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[2],
                    weirdness,
                    0.0f,
                    plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    midInlandContinentalness,
                    erosions[3],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    farInlandContinentalness,
                    erosions[3],
                    weirdness,
                    0.0f,
                    plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[4],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredBiomeOrWindsweptSavanna,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[6],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
        }
    }

    private fun addHighSlice(
        emit: (ParameterPoint, Biome) -> Unit,
        weirdness: Parameter,
    ) {
        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            for (h in humidities.indices) {
                val humidity = humidities[h]
                val middleBiome = pickMiddleBiome(t, h, weirdness)
                val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
                val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                    pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
                val plateauBiome = pickPlateauBiome(t, h, weirdness)
                val shatteredBiome = pickShatteredBiome(t, h, weirdness)
                val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
                val slopeBiome = pickSlopeBiome(t, h, weirdness)
                val peakBiome = pickPeakBiome(t, h, weirdness)
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    Parameter.span(erosions[0], erosions[1]),
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    erosions[0],
                    weirdness,
                    0.0f,
                    slopeBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[0],
                    weirdness,
                    0.0f,
                    peakBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    erosions[1],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHotOrSlopeIfCold,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[1],
                    weirdness,
                    0.0f,
                    slopeBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    Parameter.span(erosions[2], erosions[3]),
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[2],
                    weirdness,
                    0.0f,
                    plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    midInlandContinentalness,
                    erosions[3],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    farInlandContinentalness,
                    erosions[3],
                    weirdness,
                    0.0f,
                    plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[4],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    middleBiomeOrWindsweptSavanna,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, farInlandContinentalness),
                    erosions[6],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
            }
        }
    }

    private fun addMidSlice(
        emit: (ParameterPoint, Biome) -> Unit,
        weirdness: Parameter,
    ) {
        addSurfaceBiome(
            emit,
            fullRange,
            fullRange,
            coastContinentalness,
            Parameter.span(erosions[0], erosions[2]),
            weirdness,
            0.0f,
            Biomes.STONY_SHORE,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[1], temperatures[2]),
            fullRange,
            Parameter.span(nearInlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.SWAMP,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[3], temperatures[4]),
            fullRange,
            Parameter.span(nearInlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.MANGROVE_SWAMP,
        )

        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            for (h in humidities.indices) {
                val humidity = humidities[h]
                val middleBiome = pickMiddleBiome(t, h, weirdness)
                val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
                val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                    pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
                val shatteredBiome = pickShatteredBiome(t, h, weirdness)
                val plateauBiome = pickPlateauBiome(t, h, weirdness)
                val beachBiome = pickBeachBiome(t, h)
                val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
                val shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness)
                val slopeBiome = pickSlopeBiome(t, h, weirdness)
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                    erosions[0],
                    weirdness,
                    0.0f,
                    slopeBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, midInlandContinentalness),
                    erosions[1],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHotOrSlopeIfCold,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    farInlandContinentalness,
                    erosions[1],
                    weirdness,
                    0.0f,
                    if (t == 0) slopeBiome else plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    erosions[2],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    midInlandContinentalness,
                    erosions[2],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    farInlandContinentalness,
                    erosions[2],
                    weirdness,
                    0.0f,
                    plateauBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(coastContinentalness, nearInlandContinentalness),
                    erosions[3],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[3],
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                if (weirdness.max < 0L) {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        coastContinentalness,
                        erosions[4],
                        weirdness,
                        0.0f,
                        beachBiome,
                    )
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                        erosions[4],
                        weirdness,
                        0.0f,
                        middleBiome,
                    )
                } else {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        Parameter.span(coastContinentalness, farInlandContinentalness),
                        erosions[4],
                        weirdness,
                        0.0f,
                        middleBiome,
                    )
                }
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredCoastBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    erosions[5],
                    weirdness,
                    0.0f,
                    middleBiomeOrWindsweptSavanna,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredBiome,
                )
                if (weirdness.max < 0L) {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        coastContinentalness,
                        erosions[6],
                        weirdness,
                        0.0f,
                        beachBiome,
                    )
                } else {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        coastContinentalness,
                        erosions[6],
                        weirdness,
                        0.0f,
                        middleBiome,
                    )
                }
                if (t == 0) {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                        erosions[6],
                        weirdness,
                        0.0f,
                        middleBiome,
                    )
                }
            }
        }
    }

    private fun addLowSlice(
        emit: (ParameterPoint, Biome) -> Unit,
        weirdness: Parameter,
    ) {
        addSurfaceBiome(
            emit,
            fullRange,
            fullRange,
            coastContinentalness,
            Parameter.span(erosions[0], erosions[2]),
            weirdness,
            0.0f,
            Biomes.STONY_SHORE,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[1], temperatures[2]),
            fullRange,
            Parameter.span(nearInlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.SWAMP,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[3], temperatures[4]),
            fullRange,
            Parameter.span(nearInlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.MANGROVE_SWAMP,
        )

        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            for (h in humidities.indices) {
                val humidity = humidities[h]
                val middleBiome = pickMiddleBiome(t, h, weirdness)
                val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
                val middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                    pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness)
                val beachBiome = pickBeachBiome(t, h)
                val middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome)
                val shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness)
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    Parameter.span(erosions[0], erosions[1]),
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    Parameter.span(erosions[0], erosions[1]),
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHotOrSlopeIfCold,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    Parameter.span(erosions[2], erosions[3]),
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    Parameter.span(erosions[2], erosions[3]),
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    Parameter.span(erosions[3], erosions[4]),
                    weirdness,
                    0.0f,
                    beachBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                    erosions[4],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[5],
                    weirdness,
                    0.0f,
                    shatteredCoastBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    nearInlandContinentalness,
                    erosions[5],
                    weirdness,
                    0.0f,
                    middleBiomeOrWindsweptSavanna,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    erosions[5],
                    weirdness,
                    0.0f,
                    middleBiome,
                )
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    coastContinentalness,
                    erosions[6],
                    weirdness,
                    0.0f,
                    beachBiome,
                )
                if (t == 0) {
                    addSurfaceBiome(
                        emit,
                        temperature,
                        humidity,
                        Parameter.span(nearInlandContinentalness, farInlandContinentalness),
                        erosions[6],
                        weirdness,
                        0.0f,
                        middleBiome,
                    )
                }
            }
        }
    }

    private fun addValleys(
        emit: (ParameterPoint, Biome) -> Unit,
        weirdness: Parameter,
    ) {
        addSurfaceBiome(
            emit,
            frozenRange,
            fullRange,
            coastContinentalness,
            Parameter.span(erosions[0], erosions[1]),
            weirdness,
            0.0f,
            if (weirdness.max < 0L) Biomes.STONY_SHORE else Biomes.FROZEN_RIVER,
        )
        addSurfaceBiome(
            emit,
            unfrozenRange,
            fullRange,
            coastContinentalness,
            Parameter.span(erosions[0], erosions[1]),
            weirdness,
            0.0f,
            if (weirdness.max < 0L) Biomes.STONY_SHORE else Biomes.RIVER,
        )
        addSurfaceBiome(
            emit,
            frozenRange,
            fullRange,
            nearInlandContinentalness,
            Parameter.span(erosions[0], erosions[1]),
            weirdness,
            0.0f,
            Biomes.FROZEN_RIVER,
        )
        addSurfaceBiome(
            emit,
            unfrozenRange,
            fullRange,
            nearInlandContinentalness,
            Parameter.span(erosions[0], erosions[1]),
            weirdness,
            0.0f,
            Biomes.RIVER,
        )
        addSurfaceBiome(
            emit,
            frozenRange,
            fullRange,
            Parameter.span(coastContinentalness, farInlandContinentalness),
            Parameter.span(erosions[2], erosions[5]),
            weirdness,
            0.0f,
            Biomes.FROZEN_RIVER,
        )
        addSurfaceBiome(
            emit,
            unfrozenRange,
            fullRange,
            Parameter.span(coastContinentalness, farInlandContinentalness),
            Parameter.span(erosions[2], erosions[5]),
            weirdness,
            0.0f,
            Biomes.RIVER,
        )
        addSurfaceBiome(
            emit,
            frozenRange,
            fullRange,
            coastContinentalness,
            erosions[6],
            weirdness,
            0.0f,
            Biomes.FROZEN_RIVER,
        )
        addSurfaceBiome(
            emit,
            unfrozenRange,
            fullRange,
            coastContinentalness,
            erosions[6],
            weirdness,
            0.0f,
            Biomes.RIVER,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[1], temperatures[2]),
            fullRange,
            Parameter.span(inlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.SWAMP,
        )
        addSurfaceBiome(
            emit,
            Parameter.span(temperatures[3], temperatures[4]),
            fullRange,
            Parameter.span(inlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.MANGROVE_SWAMP,
        )
        addSurfaceBiome(
            emit,
            frozenRange,
            fullRange,
            Parameter.span(inlandContinentalness, farInlandContinentalness),
            erosions[6],
            weirdness,
            0.0f,
            Biomes.FROZEN_RIVER,
        )

        for (t in temperatures.indices) {
            val temperature = temperatures[t]
            for (h in humidities.indices) {
                val humidity = humidities[h]
                val middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)
                addSurfaceBiome(
                    emit,
                    temperature,
                    humidity,
                    Parameter.span(midInlandContinentalness, farInlandContinentalness),
                    Parameter.span(erosions[0], erosions[1]),
                    weirdness,
                    0.0f,
                    middleBiomeOrBadlandsIfHot,
                )
            }
        }
    }

    private fun addUndergroundBiomes(emit: (ParameterPoint, Biome) -> Unit) {
        addUndergroundBiome(
            emit,
            fullRange,
            fullRange,
            Parameter.span(0.8f, 1.0f),
            fullRange,
            fullRange,
            0.0f,
            Biomes.DRIPSTONE_CAVES,
        )
        addUndergroundBiome(
            emit,
            fullRange,
            Parameter.span(0.7f, 1.0f),
            fullRange,
            fullRange,
            fullRange,
            0.0f,
            Biomes.LUSH_CAVES,
        )
        addUndergroundBiome(
            emit,
            fullRange,
            fullRange,
            Parameter.span(coastContinentalness, inlandContinentalness),
            Parameter.span(erosions[5], erosions[6]),
            Parameter.span(-1.1f, -0.85f),
            0.0f,
            Biomes.SULFUR_CAVES,
        )
        addBottomBiome(
            emit,
            fullRange,
            fullRange,
            fullRange,
            Parameter.span(erosions[0], erosions[1]),
            fullRange,
            0.0f,
            Biomes.DEEP_DARK,
        )
    }

    private fun pickMiddleBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (weirdness.max < 0L) return middleBiomes[t][h]
        return middleBiomesVariant[t][h] ?: middleBiomes[t][h]
    }

    private fun pickMiddleBiomeOrBadlandsIfHot(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = if (t == 4) pickBadlandsBiome(h, weirdness) else pickMiddleBiome(t, h, weirdness)

    private fun pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = if (t == 0) pickSlopeBiome(t, h, weirdness) else pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness)

    private fun maybePickWindsweptSavannaBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
        underlyingBiome: Biome,
    ): Biome = if (t > 1 && h < 4 && weirdness.max >= 0L) Biomes.WINDSWEPT_SAVANNA else underlyingBiome

    private fun pickShatteredCoastBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        val beachOrMiddleBiome = if (weirdness.max >= 0L) pickMiddleBiome(t, h, weirdness) else pickBeachBiome(t, h)
        return maybePickWindsweptSavannaBiome(t, h, weirdness, beachOrMiddleBiome)
    }

    private fun pickBeachBiome(
        t: Int,
        h: Int,
    ): Biome {
        if (t == 0) return Biomes.SNOWY_BEACH
        return if (t == 4) Biomes.DESERT else Biomes.BEACH
    }

    private fun pickBadlandsBiome(
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (h < 2) return if (weirdness.max < 0L) Biomes.BADLANDS else Biomes.ERODED_BADLANDS
        return if (h < 3) Biomes.BADLANDS else Biomes.WOODED_BADLANDS
    }

    private fun pickPlateauBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (weirdness.max >= 0L) {
            plateauBiomesVariant[t][h]?.let { return it }
        }
        return plateauBiomes[t][h]
    }

    private fun pickPeakBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (t <= 2) return if (weirdness.max < 0L) Biomes.JAGGED_PEAKS else Biomes.FROZEN_PEAKS
        return if (t == 3) Biomes.STONY_PEAKS else pickBadlandsBiome(h, weirdness)
    }

    private fun pickSlopeBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome {
        if (t >= 3) return pickPlateauBiome(t, h, weirdness)
        return if (h <= 1) Biomes.SNOWY_SLOPES else Biomes.GROVE
    }

    private fun pickShatteredBiome(
        t: Int,
        h: Int,
        weirdness: Parameter,
    ): Biome = shatteredBiomes[t][h] ?: pickMiddleBiome(t, h, weirdness)

    private fun addSurfaceBiome(
        emit: (ParameterPoint, Biome) -> Unit,
        temperature: Parameter,
        humidity: Parameter,
        continentalness: Parameter,
        erosion: Parameter,
        weirdness: Parameter,
        offset: Float,
        biome: Biome,
    ) {
        emit(
            ParameterPoint(
                temperature,
                humidity,
                continentalness,
                erosion,
                Parameter.point(0.0f),
                weirdness,
                Parameter.quantizeCoord(offset),
            ),
            biome,
        )
        emit(
            ParameterPoint(
                temperature,
                humidity,
                continentalness,
                erosion,
                Parameter.point(1.0f),
                weirdness,
                Parameter.quantizeCoord(offset),
            ),
            biome,
        )
    }

    private fun addUndergroundBiome(
        emit: (ParameterPoint, Biome) -> Unit,
        temperature: Parameter,
        humidity: Parameter,
        continentalness: Parameter,
        erosion: Parameter,
        weirdness: Parameter,
        offset: Float,
        biome: Biome,
    ) {
        emit(
            ParameterPoint(
                temperature,
                humidity,
                continentalness,
                erosion,
                Parameter.span(0.2f, 0.9f),
                weirdness,
                Parameter.quantizeCoord(offset),
            ),
            biome,
        )
    }

    private fun addBottomBiome(
        emit: (ParameterPoint, Biome) -> Unit,
        temperature: Parameter,
        humidity: Parameter,
        continentalness: Parameter,
        erosion: Parameter,
        weirdness: Parameter,
        offset: Float,
        biome: Biome,
    ) {
        emit(
            ParameterPoint(
                temperature,
                humidity,
                continentalness,
                erosion,
                Parameter.point(1.1f),
                weirdness,
                Parameter.quantizeCoord(offset),
            ),
            biome,
        )
    }
}
