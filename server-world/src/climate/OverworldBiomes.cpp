#include <basalt/server/world/climate/OverworldBiomes.hpp>

#include <utility>

namespace basalt::server::world::climate::overworldBiomes {

Tables::Tables()
    : fullRange(parameterSpan(-1.0F, 1.0F)),
      temperatures{parameterSpan(-1.0F, -0.45F), parameterSpan(-0.45F, -0.15F), parameterSpan(-0.15F, 0.2F),
                   parameterSpan(0.2F, 0.55F), parameterSpan(0.55F, 1.0F)},
      humidities{parameterSpan(-1.0F, -0.35F), parameterSpan(-0.35F, -0.1F), parameterSpan(-0.1F, 0.1F),
                 parameterSpan(0.1F, 0.3F), parameterSpan(0.3F, 1.0F)},
      erosions{parameterSpan(-1.0F, -0.78F),   parameterSpan(-0.78F, -0.375F), parameterSpan(-0.375F, -0.2225F),
               parameterSpan(-0.2225F, 0.05F), parameterSpan(0.05F, 0.45F),    parameterSpan(0.45F, 0.55F),
               parameterSpan(0.55F, 1.0F)},
      frozenRange(parameterSpan(-1.0F, -0.45F)),
      unfrozenRange(Parameter{parameterSpan(-0.45F, -0.15F).min, parameterSpan(0.55F, 1.0F).max}),
      mushroomFieldsContinentalness(parameterSpan(-1.2F, -1.05F)),
      deepOceanContinentalness(parameterSpan(-1.05F, -0.455F)), oceanContinentalness(parameterSpan(-0.455F, -0.19F)),
      coastContinentalness(parameterSpan(-0.19F, -0.11F)), inlandContinentalness(parameterSpan(-0.11F, 0.55F)),
      nearInlandContinentalness(parameterSpan(-0.11F, 0.03F)), midInlandContinentalness(parameterSpan(0.03F, 0.3F)),
      farInlandContinentalness(parameterSpan(0.3F, 1.0F)),
      oceans{{{"deep_frozen_ocean", "deep_cold_ocean", "deep_ocean", "deep_lukewarm_ocean", "warm_ocean"},
              {"frozen_ocean", "cold_ocean", "ocean", "lukewarm_ocean", "warm_ocean"}}},
      middleBiomes{{{"snowy_plains", "snowy_plains", "snowy_plains", "snowy_taiga", "taiga"},
                    {"plains", "plains", "forest", "taiga", "old_growth_spruce_taiga"},
                    {"flower_forest", "plains", "forest", "birch_forest", "dark_forest"},
                    {"savanna", "savanna", "forest", "jungle", "jungle"},
                    {"desert", "desert", "desert", "desert", "desert"}}},
      middleBiomesVariant{{{"ice_spikes", std::nullopt, "snowy_taiga", std::nullopt, std::nullopt},
                           {"dappled_forest", std::nullopt, std::nullopt, std::nullopt, "old_growth_pine_taiga"},
                           {"sunflower_plains", std::nullopt, std::nullopt, "old_growth_birch_forest", std::nullopt},
                           {std::nullopt, std::nullopt, "plains", "sparse_jungle", "bamboo_jungle"},
                           {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}}},
      plateauBiomes{{{"snowy_plains", "snowy_plains", "snowy_plains", "snowy_taiga", "snowy_taiga"},
                     {"meadow", "meadow", "forest", "taiga", "old_growth_spruce_taiga"},
                     {"meadow", "meadow", "meadow", "meadow", "pale_garden"},
                     {"savanna_plateau", "savanna_plateau", "forest", "forest", "jungle"},
                     {"badlands", "badlands", "badlands", "wooded_badlands", "wooded_badlands"}}},
      plateauBiomesVariant{{{"ice_spikes", std::nullopt, std::nullopt, std::nullopt, std::nullopt},
                            {"cherry_grove", std::nullopt, "meadow", "meadow", "old_growth_pine_taiga"},
                            {"cherry_grove", "cherry_grove", "forest", "birch_forest", std::nullopt},
                            {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
                            {"eroded_badlands", "eroded_badlands", std::nullopt, std::nullopt, std::nullopt}}},
      shatteredBiomes{
          {{"windswept_gravelly_hills", "windswept_gravelly_hills", "windswept_hills", "windswept_forest",
            "windswept_forest"},
           {"windswept_gravelly_hills", "windswept_gravelly_hills", "windswept_hills", "windswept_forest",
            "windswept_forest"},
           {"windswept_hills", "windswept_hills", "windswept_hills", "windswept_forest", "windswept_forest"},
           {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt},
           {std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}}} {}

BiomeName Tables::pickMiddleBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    if (weirdness.max < 0) {
        return middleBiomes[t][h];
    }
    return middleBiomesVariant[t][h].value_or(middleBiomes[t][h]);
}

BiomeName Tables::pickMiddleBiomeOrBadlandsIfHot(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    return t == 4 ? pickBadlandsBiome(h, weirdness) : pickMiddleBiome(t, h, weirdness);
}

BiomeName Tables::pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(std::size_t t, std::size_t h,
                                                              const Parameter& weirdness) const {
    return t == 0 ? pickSlopeBiome(t, h, weirdness) : pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
}

BiomeName Tables::maybePickWindsweptSavannaBiome(std::size_t t, std::size_t h, const Parameter& weirdness,
                                                 BiomeName underlying) const {
    return t > 1 && h < 4 && weirdness.max >= 0 ? "windswept_savanna" : underlying;
}

BiomeName Tables::pickShatteredCoastBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    const BiomeName beachOrMiddle = weirdness.max >= 0 ? pickMiddleBiome(t, h, weirdness) : pickBeachBiome(t, h);
    return maybePickWindsweptSavannaBiome(t, h, weirdness, beachOrMiddle);
}

BiomeName Tables::pickBeachBiome(std::size_t t, std::size_t) {
    if (t == 0) {
        return "snowy_beach";
    }
    return t == 4 ? "desert" : "beach";
}

BiomeName Tables::pickBadlandsBiome(std::size_t h, const Parameter& weirdness) {
    if (h < 2) {
        return weirdness.max < 0 ? "badlands" : "eroded_badlands";
    }
    return h < 3 ? "badlands" : "wooded_badlands";
}

BiomeName Tables::pickPlateauBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    if (weirdness.max >= 0 && plateauBiomesVariant[t][h].has_value()) {
        return *plateauBiomesVariant[t][h];
    }
    return plateauBiomes[t][h];
}

BiomeName Tables::pickPeakBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    if (t <= 2) {
        return weirdness.max < 0 ? "jagged_peaks" : "frozen_peaks";
    }
    return t == 3 ? "stony_peaks" : pickBadlandsBiome(h, weirdness);
}

BiomeName Tables::pickSlopeBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    if (t >= 3) {
        return pickPlateauBiome(t, h, weirdness);
    }
    return h <= 1 ? "snowy_slopes" : "grove";
}

BiomeName Tables::pickShatteredBiome(std::size_t t, std::size_t h, const Parameter& weirdness) const {
    return shatteredBiomes[t][h].value_or(pickMiddleBiome(t, h, weirdness));
}

void Tables::addSurfaceBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                             const Parameter& continentalness, const Parameter& erosion, const Parameter& weirdness,
                             float offset, BiomeName biome) {
    // Two points, one at each end of the depth axis: that is what makes a biome a surface one.
    emit({temperature, humidity, continentalness, erosion, parameterPointAt(0.0F), weirdness, quantizeCoord(offset)},
         biome);
    emit({temperature, humidity, continentalness, erosion, parameterPointAt(1.0F), weirdness, quantizeCoord(offset)},
         biome);
}

void Tables::addUndergroundBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                                 const Parameter& continentalness, const Parameter& erosion, const Parameter& weirdness,
                                 float offset, BiomeName biome) {
    emit({temperature, humidity, continentalness, erosion, parameterSpan(0.2F, 0.9F), weirdness, quantizeCoord(offset)},
         biome);
}

void Tables::addBottomBiome(const Emit& emit, const Parameter& temperature, const Parameter& humidity,
                            const Parameter& continentalness, const Parameter& erosion, const Parameter& weirdness,
                            float offset, BiomeName biome) {
    emit({temperature, humidity, continentalness, erosion, parameterPointAt(1.1F), weirdness, quantizeCoord(offset)},
         biome);
}

void Tables::addMidSlice(const Emit& emit, const Parameter& weirdness) const {

    addSurfaceBiome(emit, fullRange, fullRange, coastContinentalness, Parameter{erosions[0].min, erosions[2].max},
                    weirdness, 0.0F, "stony_shore");
    addSurfaceBiome(emit, Parameter{temperatures[1].min, temperatures[2].max}, fullRange,
                    Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                    0.0F, "swamp");
    addSurfaceBiome(emit, Parameter{temperatures[3].min, temperatures[4].max}, fullRange,
                    Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                    0.0F, "mangrove_swamp");

    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        for (std::size_t h = 0; h < humidities.size(); ++h) {
            const auto humidity = humidities[h];
            const auto middleBiome = pickMiddleBiome(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const auto shatteredBiome = pickShatteredBiome(t, h, weirdness);
            const auto plateauBiome = pickPlateauBiome(t, h, weirdness);
            const auto beachBiome = pickBeachBiome(t, h);
            const auto middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome);
            const auto shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness);
            const auto slopeBiome = pickSlopeBiome(t, h, weirdness);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[0],
                            weirdness, 0.0F, slopeBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{nearInlandContinentalness.min, midInlandContinentalness.max}, erosions[1],
                            weirdness, 0.0F, middleBiomeOrBadlandsIfHotOrSlopeIfCold);
            addSurfaceBiome(emit, temperature, humidity, farInlandContinentalness, erosions[1], weirdness, 0.0F,
                            (t == 0 ? slopeBiome : plateauBiome));
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness, erosions[2], weirdness, 0.0F,
                            middleBiome);
            addSurfaceBiome(emit, temperature, humidity, midInlandContinentalness, erosions[2], weirdness, 0.0F,
                            middleBiomeOrBadlandsIfHot);
            addSurfaceBiome(emit, temperature, humidity, farInlandContinentalness, erosions[2], weirdness, 0.0F,
                            plateauBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max}, erosions[3], weirdness,
                            0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[3],
                            weirdness, 0.0F, middleBiomeOrBadlandsIfHot);
            if (weirdness.max < 0) {
                addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[4], weirdness, 0.0F,
                                beachBiome);
                addSurfaceBiome(emit, temperature, humidity,
                                Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[4],
                                weirdness, 0.0F, middleBiome);
            } else {
                addSurfaceBiome(emit, temperature, humidity,
                                Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[4],
                                weirdness, 0.0F, middleBiome);
            }
            addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[5], weirdness, 0.0F,
                            shatteredCoastBiome);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness, erosions[5], weirdness, 0.0F,
                            middleBiomeOrWindsweptSavanna);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[5],
                            weirdness, 0.0F, shatteredBiome);
            if (weirdness.max < 0) {
                addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[6], weirdness, 0.0F,
                                beachBiome);
            } else {
                addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[6], weirdness, 0.0F,
                                middleBiome);
            }
            if (t == 0) {
                addSurfaceBiome(emit, temperature, humidity,
                                Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6],
                                weirdness, 0.0F, middleBiome);
            }
        }
    }
}

void Tables::addLowSlice(const Emit& emit, const Parameter& weirdness) const {

    addSurfaceBiome(emit, fullRange, fullRange, coastContinentalness, Parameter{erosions[0].min, erosions[2].max},
                    weirdness, 0.0F, "stony_shore");
    addSurfaceBiome(emit, Parameter{temperatures[1].min, temperatures[2].max}, fullRange,
                    Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                    0.0F, "swamp");
    addSurfaceBiome(emit, Parameter{temperatures[3].min, temperatures[4].max}, fullRange,
                    Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                    0.0F, "mangrove_swamp");

    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        for (std::size_t h = 0; h < humidities.size(); ++h) {
            const auto humidity = humidities[h];
            const auto middleBiome = pickMiddleBiome(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const auto beachBiome = pickBeachBiome(t, h);
            const auto middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome);
            const auto shatteredCoastBiome = pickShatteredCoastBiome(t, h, weirdness);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness,
                            Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, middleBiomeOrBadlandsIfHot);
            addSurfaceBiome(
                emit, temperature, humidity, Parameter{midInlandContinentalness.min, farInlandContinentalness.max},
                Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, middleBiomeOrBadlandsIfHotOrSlopeIfCold);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness,
                            Parameter{erosions[2].min, erosions[3].max}, weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max},
                            Parameter{erosions[2].min, erosions[3].max}, weirdness, 0.0F, middleBiomeOrBadlandsIfHot);
            addSurfaceBiome(emit, temperature, humidity, coastContinentalness,
                            Parameter{erosions[3].min, erosions[4].max}, weirdness, 0.0F, beachBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[4],
                            weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[5], weirdness, 0.0F,
                            shatteredCoastBiome);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness, erosions[5], weirdness, 0.0F,
                            middleBiomeOrWindsweptSavanna);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[5],
                            weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity, coastContinentalness, erosions[6], weirdness, 0.0F,
                            beachBiome);
            if (t == 0) {
                addSurfaceBiome(emit, temperature, humidity,
                                Parameter{nearInlandContinentalness.min, farInlandContinentalness.max}, erosions[6],
                                weirdness, 0.0F, middleBiome);
            }
        }
    }
}

void Tables::addPeaks(const Emit& emit, const Parameter& weirdness) const {

    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        for (std::size_t h = 0; h < humidities.size(); ++h) {
            const auto humidity = humidities[h];
            const auto middleBiome = pickMiddleBiome(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const auto plateauBiome = pickPlateauBiome(t, h, weirdness);
            const auto shatteredBiome = pickShatteredBiome(t, h, weirdness);
            const auto shatteredBiomeOrWindsweptSavanna =
                maybePickWindsweptSavannaBiome(t, h, weirdness, shatteredBiome);
            const auto peakBiome = pickPeakBiome(t, h, weirdness);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[0], weirdness,
                            0.0F, peakBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max}, erosions[1], weirdness,
                            0.0F, middleBiomeOrBadlandsIfHotOrSlopeIfCold);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[1],
                            weirdness, 0.0F, peakBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max},
                            Parameter{erosions[2].min, erosions[3].max}, weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[2],
                            weirdness, 0.0F, plateauBiome);
            addSurfaceBiome(emit, temperature, humidity, midInlandContinentalness, erosions[3], weirdness, 0.0F,
                            middleBiomeOrBadlandsIfHot);
            addSurfaceBiome(emit, temperature, humidity, farInlandContinentalness, erosions[3], weirdness, 0.0F,
                            plateauBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[4], weirdness,
                            0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max}, erosions[5], weirdness,
                            0.0F, shatteredBiomeOrWindsweptSavanna);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[5],
                            weirdness, 0.0F, shatteredBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                            0.0F, middleBiome);
        }
    }
}

void Tables::addHighSlice(const Emit& emit, const Parameter& weirdness) const {

    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        for (std::size_t h = 0; h < humidities.size(); ++h) {
            const auto humidity = humidities[h];
            const auto middleBiome = pickMiddleBiome(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const auto middleBiomeOrBadlandsIfHotOrSlopeIfCold =
                pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const auto plateauBiome = pickPlateauBiome(t, h, weirdness);
            const auto shatteredBiome = pickShatteredBiome(t, h, weirdness);
            const auto middleBiomeOrWindsweptSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middleBiome);
            const auto slopeBiome = pickSlopeBiome(t, h, weirdness);
            const auto peakBiome = pickPeakBiome(t, h, weirdness);
            addSurfaceBiome(emit, temperature, humidity, coastContinentalness,
                            Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness, erosions[0], weirdness, 0.0F,
                            slopeBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[0],
                            weirdness, 0.0F, peakBiome);
            addSurfaceBiome(emit, temperature, humidity, nearInlandContinentalness, erosions[1], weirdness, 0.0F,
                            middleBiomeOrBadlandsIfHotOrSlopeIfCold);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[1],
                            weirdness, 0.0F, slopeBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max},
                            Parameter{erosions[2].min, erosions[3].max}, weirdness, 0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[2],
                            weirdness, 0.0F, plateauBiome);
            addSurfaceBiome(emit, temperature, humidity, midInlandContinentalness, erosions[3], weirdness, 0.0F,
                            middleBiomeOrBadlandsIfHot);
            addSurfaceBiome(emit, temperature, humidity, farInlandContinentalness, erosions[3], weirdness, 0.0F,
                            plateauBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[4], weirdness,
                            0.0F, middleBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, nearInlandContinentalness.max}, erosions[5], weirdness,
                            0.0F, middleBiomeOrWindsweptSavanna);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max}, erosions[5],
                            weirdness, 0.0F, shatteredBiome);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{coastContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness,
                            0.0F, middleBiome);
        }
    }
}

void Tables::addValleys(const Emit& emit, const Parameter& weirdness) const {

    addSurfaceBiome(emit, frozenRange, fullRange, coastContinentalness, Parameter{erosions[0].min, erosions[1].max},
                    weirdness, 0.0F, (weirdness.max < 0 ? "stony_shore" : "frozen_river"));
    addSurfaceBiome(emit, unfrozenRange, fullRange, coastContinentalness, Parameter{erosions[0].min, erosions[1].max},
                    weirdness, 0.0F, (weirdness.max < 0 ? "stony_shore" : "river"));
    addSurfaceBiome(emit, frozenRange, fullRange, nearInlandContinentalness,
                    Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, "frozen_river");
    addSurfaceBiome(emit, unfrozenRange, fullRange, nearInlandContinentalness,
                    Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, "river");
    addSurfaceBiome(emit, frozenRange, fullRange, Parameter{coastContinentalness.min, farInlandContinentalness.max},
                    Parameter{erosions[2].min, erosions[5].max}, weirdness, 0.0F, "frozen_river");
    addSurfaceBiome(emit, unfrozenRange, fullRange, Parameter{coastContinentalness.min, farInlandContinentalness.max},
                    Parameter{erosions[2].min, erosions[5].max}, weirdness, 0.0F, "river");
    addSurfaceBiome(emit, frozenRange, fullRange, coastContinentalness, erosions[6], weirdness, 0.0F, "frozen_river");
    addSurfaceBiome(emit, unfrozenRange, fullRange, coastContinentalness, erosions[6], weirdness, 0.0F, "river");
    addSurfaceBiome(emit, Parameter{temperatures[1].min, temperatures[2].max}, fullRange,
                    Parameter{inlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness, 0.0F,
                    "swamp");
    addSurfaceBiome(emit, Parameter{temperatures[3].min, temperatures[4].max}, fullRange,
                    Parameter{inlandContinentalness.min, farInlandContinentalness.max}, erosions[6], weirdness, 0.0F,
                    "mangrove_swamp");
    addSurfaceBiome(emit, frozenRange, fullRange, Parameter{inlandContinentalness.min, farInlandContinentalness.max},
                    erosions[6], weirdness, 0.0F, "frozen_river");

    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        for (std::size_t h = 0; h < humidities.size(); ++h) {
            const auto humidity = humidities[h];
            const auto middleBiomeOrBadlandsIfHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            addSurfaceBiome(emit, temperature, humidity,
                            Parameter{midInlandContinentalness.min, farInlandContinentalness.max},
                            Parameter{erosions[0].min, erosions[1].max}, weirdness, 0.0F, middleBiomeOrBadlandsIfHot);
        }
    }
}

void Tables::addOffCoastBiomes(const Emit& emit) const {

    addSurfaceBiome(emit, fullRange, fullRange, mushroomFieldsContinentalness, fullRange, fullRange, 0.0F,
                    "mushroom_fields");
    for (std::size_t t = 0; t < temperatures.size(); ++t) {
        const auto temperature = temperatures[t];
        addSurfaceBiome(emit, temperature, fullRange, deepOceanContinentalness, fullRange, fullRange, 0.0F,
                        oceans[0][t]);
        addSurfaceBiome(emit, temperature, fullRange, oceanContinentalness, fullRange, fullRange, 0.0F, oceans[1][t]);
    }
}

void Tables::addUndergroundBiomes(const Emit& emit) const {

    addUndergroundBiome(emit, fullRange, fullRange, parameterSpan(0.8F, 1.0F), fullRange, fullRange, 0.0F,
                        "dripstone_caves");
    addUndergroundBiome(emit, fullRange, parameterSpan(0.7F, 1.0F), fullRange, fullRange, fullRange, 0.0F,
                        "lush_caves");
    addUndergroundBiome(emit, fullRange, fullRange, Parameter{coastContinentalness.min, inlandContinentalness.max},
                        Parameter{erosions[5].min, erosions[6].max}, parameterSpan(-1.1F, -0.85F), 0.0F,
                        "sulfur_caves");
    addBottomBiome(emit, fullRange, fullRange, fullRange, Parameter{erosions[0].min, erosions[1].max}, fullRange, 0.0F,
                   "deep_dark");
}

std::vector<std::pair<ParameterPoint, BiomeName>> build() {
    const Tables tables;
    std::vector<std::pair<ParameterPoint, BiomeName>> values;
    const Emit emit = [&values](const ParameterPoint& point, BiomeName biome) { values.emplace_back(point, biome); };

    // The order is vanilla's own, and it is semantically significant: ties between equally good
    // points go to whichever was registered first.
    tables.addOffCoastBiomes(emit);
    tables.addMidSlice(emit, parameterSpan(-1.0F, -0.93333334F));
    tables.addHighSlice(emit, parameterSpan(-0.93333334F, -0.7666667F));
    tables.addPeaks(emit, parameterSpan(-0.7666667F, -0.56666666F));
    tables.addHighSlice(emit, parameterSpan(-0.56666666F, -0.4F));
    tables.addMidSlice(emit, parameterSpan(-0.4F, -0.26666668F));
    tables.addLowSlice(emit, parameterSpan(-0.26666668F, -0.05F));
    tables.addValleys(emit, parameterSpan(-0.05F, 0.05F));
    tables.addLowSlice(emit, parameterSpan(0.05F, 0.26666668F));
    tables.addMidSlice(emit, parameterSpan(0.26666668F, 0.4F));
    tables.addHighSlice(emit, parameterSpan(0.4F, 0.56666666F));
    tables.addPeaks(emit, parameterSpan(0.56666666F, 0.7666667F));
    tables.addHighSlice(emit, parameterSpan(0.7666667F, 0.93333334F));
    tables.addMidSlice(emit, parameterSpan(0.93333334F, 1.0F));
    tables.addUndergroundBiomes(emit);
    return values;
}

} // namespace basalt::server::world::climate::overworldBiomes
