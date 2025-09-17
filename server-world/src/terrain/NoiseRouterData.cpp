#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/TerrainProvider.hpp>

#include <utility>
#include <vector>

namespace basalt::server::world::terrain {
namespace fn = densityfunction::functions;

namespace {

// parity: NoiseRouterData.GLOBAL_OFFSET.
// The source literal is a float promoted into DensityFunctions.constant(double).
constexpr double kGlobalOffset = -0.5037500262260437;

// parity: NoiseRouterData.peaksAndValleys(DensityFunction).
[[nodiscard]] Function peaksAndValleys(Function weirdness) {
    return fn::mul(fn::add(fn::abs(fn::add(fn::abs(std::move(weirdness)), fn::constant(-0.6666666666666666))),
                           fn::constant(-0.3333333333333333)),
                   fn::constant(-3.0));
}

// parity: NoiseRouterData.offsetToDepth(DensityFunction).
[[nodiscard]] Function offsetToDepth(Function offset) {
    return fn::add(fn::yClampedGradient(-64, 320, 1.5, -1.5), std::move(offset));
}

// parity: NoiseRouterData.splineWithBlending(DensityFunction, DensityFunction).
[[nodiscard]] Function splineWithBlending(Function spline, Function blendingTarget) {
    return fn::flatCache(fn::cache2d(fn::lerp(fn::blendAlpha(), std::move(blendingTarget), std::move(spline))));
}

// parity: NoiseRouterData.noiseGradientDensity(DensityFunction, DensityFunction).
[[nodiscard]] Function noiseGradientDensity(Function factor, Function depthWithJaggedness) {
    Function gradientUnscaled = fn::mul(std::move(depthWithJaggedness), std::move(factor));
    return fn::mul(fn::constant(4.0), fn::quarterNegative(std::move(gradientUnscaled)));
}

[[nodiscard]] Function remap(Function input, double fromMin, double fromMax, double toMin, double toMax) {
    const double factor = (toMax - toMin) / (fromMax - fromMin);
    const double offset = toMin - fromMin * factor;
    return fn::add(fn::mul(std::move(input), fn::constant(factor)), fn::constant(offset));
}

// parity: NoiseRouterData.slide(DensityFunction, int, int, int, int, double, int, int, double).
[[nodiscard]] Function slide(Function caves, std::int32_t minY, std::int32_t height, std::int32_t topStartY,
                             std::int32_t topEndY, double topTarget, std::int32_t bottomStartY, std::int32_t bottomEndY,
                             double bottomTarget) {
    Function topFactor = fn::yClampedGradient(minY + height - topStartY, minY + height - topEndY, 1.0, 0.0);
    Function noiseValue = fn::lerp(std::move(topFactor), topTarget, std::move(caves));
    Function bottomFactor = fn::yClampedGradient(minY + bottomStartY, minY + bottomEndY, 0.0, 1.0);
    return fn::lerp(std::move(bottomFactor), bottomTarget, std::move(noiseValue));
}

// parity: NoiseRouterData.slideOverworld(boolean, DensityFunction). Non-amplified only.
[[nodiscard]] Function slideOverworld(Function caves) {
    return slide(std::move(caves), -64, 384, 80, 64, -0.078125, 0, 24, 0.1171875);
}

// parity: NoiseRouterData.postProcess(DensityFunction).
[[nodiscard]] Function postProcess(Function slideResult) {
    Function blended = fn::blendDensity(std::move(slideResult));
    return fn::squeeze(fn::interpolated(fn::mul(std::move(blended), fn::constant(0.64))));
}

// parity: NoiseRouterData.preliminarySurfaceLevel, non-amplified Overworld.
[[nodiscard]] Function preliminarySurfaceLevel(Function offset, Function factor) {
    Function cachedFactor = fn::cache2d(std::move(factor));
    Function cachedOffset = fn::cache2d(std::move(offset));
    Function upperBound = fn::clamp(remap(fn::add(fn::mul(fn::constant(0.2734375), fn::invert(cachedFactor)),
                                                  fn::mul(fn::constant(-1.0), cachedOffset)),
                                          1.5, -1.5, -64.0, 320.0),
                                    -40.0, 320.0);
    Function density =
        fn::add(slideOverworld(fn::clamp(
                    fn::add(noiseGradientDensity(cachedFactor, offsetToDepth(cachedOffset)), fn::constant(-0.703125)),
                    -64.0, 64.0)),
                fn::constant(-0.390625));
    return fn::findTopSurface(std::move(density), std::move(upperBound), -64, 8);
}

// parity: NoiseRouterData.QuantizedSpaghettiRarity.
[[nodiscard]] Function wrapSpaghettiRarity(Function input, std::shared_ptr<const noise::NormalNoise> targetNoise,
                                           std::vector<double> thresholds, const std::vector<double>& rarities) {
    std::vector<Function> scaled;
    scaled.reserve(rarities.size());
    for (const double rarity : rarities) {
        scaled.push_back(fn::mul(fn::constant(rarity), fn::noise(targetNoise, 1.0 / rarity, 1.0 / rarity)));
    }
    return fn::abs(fn::intervalSelect(std::move(input), std::move(thresholds), std::move(scaled)));
}

[[nodiscard]] Function wrapSpaghettiRarity2d(Function rarity, std::shared_ptr<const noise::NormalNoise> targetNoise) {
    return wrapSpaghettiRarity(std::move(rarity), std::move(targetNoise), {-0.75, -0.5, 0.5, 0.75},
                               {0.5, 0.75, 1.0, 2.0, 3.0});
}

[[nodiscard]] Function wrapSpaghettiRarity3d(Function rarity, std::shared_ptr<const noise::NormalNoise> targetNoise) {
    return wrapSpaghettiRarity(std::move(rarity), std::move(targetNoise), {-0.5, 0.0, 0.5}, {0.75, 1.0, 1.5, 2.0});
}

[[nodiscard]] Function yLimitedInterpolatable(Function y, Function whenInRange, std::int32_t minYInclusive,
                                              std::int32_t maxYInclusive, double whenOutOfRange) {
    return fn::interpolated(fn::rangeChoice(std::move(y), static_cast<double>(minYInclusive),
                                            static_cast<double>(maxYInclusive + 1), std::move(whenInRange),
                                            fn::constant(whenOutOfRange)));
}

// parity: NoiseRouterData.spaghettiRoughnessFunction.
[[nodiscard]] Function spaghettiRoughness(const NoiseLookup& noise) {
    Function roughness = fn::noise(noise("spaghetti_roughness"));
    Function modulator = fn::mappedNoise(noise("spaghetti_roughness_modulator"), 1.0, 1.0, 0.0, -0.1);
    return fn::cacheOnce(fn::mul(std::move(modulator), fn::add(fn::abs(std::move(roughness)), fn::constant(-0.4))));
}

// parity: NoiseRouterData.entrances.
[[nodiscard]] Function entrances(const NoiseLookup& noise, Function roughness) {
    Function rarity = fn::cacheOnce(fn::noise(noise("spaghetti_3d_rarity"), 2.0, 1.0));
    Function thickness = fn::mappedNoise(noise("spaghetti_3d_thickness"), 1.0, 1.0, -0.065, -0.088);
    Function cave1 = wrapSpaghettiRarity3d(rarity, noise("spaghetti_3d_1"));
    Function cave2 = wrapSpaghettiRarity3d(std::move(rarity), noise("spaghetti_3d_2"));
    Function spaghetti =
        fn::clamp(fn::add(fn::max(std::move(cave1), std::move(cave2)), std::move(thickness)), -1.0, 1.0);
    Function largeEntrance = fn::add(fn::add(fn::noise(noise("cave_entrance"), 0.75, 0.5), fn::constant(0.37)),
                                     fn::yClampedGradient(-10, 30, 0.3, 0.0));
    return fn::cacheOnce(fn::min(std::move(largeEntrance), fn::add(std::move(roughness), std::move(spaghetti))));
}

// parity: NoiseRouterData.spaghetti2D.
[[nodiscard]] Function spaghetti2d(const NoiseLookup& noise, Function thickness) {
    Function rarity = fn::noise(noise("spaghetti_2d_modulator"), 2.0, 1.0);
    Function cave = wrapSpaghettiRarity2d(std::move(rarity), noise("spaghetti_2d"));
    Function elevation = fn::mappedNoise(noise("spaghetti_2d_elevation"), 1.0, 0.0, -8.0, 8.0);
    Function sloped = fn::abs(fn::add(fn::flatCache(std::move(elevation)), fn::yClampedGradient(-64, 320, 8.0, -40.0)));
    Function layerRidged = fn::cube(fn::add(std::move(sloped), thickness));
    Function caveNoise = fn::add(std::move(cave), fn::mul(fn::constant(0.083), std::move(thickness)));
    return fn::clamp(fn::max(std::move(caveNoise), std::move(layerRidged)), -1.0, 1.0);
}

// parity: NoiseRouterData.pillars.
[[nodiscard]] Function pillars(const NoiseLookup& noise) {
    Function source = fn::noise(noise("pillar"), 25.0, 0.3);
    Function rareness = fn::mappedNoise(noise("pillar_rareness"), 1.0, 1.0, 0.0, -2.0);
    Function thickness = fn::mappedNoise(noise("pillar_thickness"), 1.0, 1.0, 0.0, 1.1);
    Function withRareness = fn::add(fn::mul(std::move(source), fn::constant(2.0)), std::move(rareness));
    return fn::cacheOnce(fn::mul(std::move(withRareness), fn::cube(std::move(thickness))));
}

// parity: NoiseRouterData.noodle.
[[nodiscard]] Function noodle(const NoiseLookup& noise) {
    Function y = fn::yClampedGradient(-4064, 4062, -4064.0, 4062.0);
    Function toggle = yLimitedInterpolatable(y, fn::noise(noise("noodle")), -60, 320, -1.0);
    Function thickness =
        yLimitedInterpolatable(y, fn::mappedNoise(noise("noodle_thickness"), 1.0, 1.0, -0.05, -0.1), -60, 320, 0.0);
    Function ridgeA = yLimitedInterpolatable(
        y, fn::noise(noise("noodle_ridge_a"), 2.6666666666666665, 2.6666666666666665), -60, 320, 0.0);
    Function ridgeB = yLimitedInterpolatable(
        std::move(y), fn::noise(noise("noodle_ridge_b"), 2.6666666666666665, 2.6666666666666665), -60, 320, 0.0);
    Function ridged = fn::mul(fn::constant(1.5), fn::max(fn::abs(std::move(ridgeA)), fn::abs(std::move(ridgeB))));
    return fn::rangeChoice(std::move(toggle), -1'000'000.0, 0.0, fn::constant(64.0),
                           fn::add(std::move(thickness), std::move(ridged)));
}

// parity: NoiseRouterData.underground.
[[nodiscard]] Function underground(const NoiseLookup& noise, Function slopedCheese, Function spaghetti,
                                   Function roughness, Function entranceDensity, Function pillarSource) {
    Function layer = fn::noise(noise("cave_layer"), 1.0, 8.0);
    Function layerized = fn::mul(fn::constant(4.0), fn::square(std::move(layer)));
    Function cheese = fn::noise(noise("cave_cheese"), 1.0, 0.6666666666666666);
    Function solidifiedCheese =
        fn::add(fn::clamp(fn::add(fn::constant(0.27), std::move(cheese)), -1.0, 1.0),
                fn::clamp(fn::add(fn::constant(1.5), fn::mul(fn::constant(-0.64), std::move(slopedCheese))), 0.0, 0.5));
    Function baseCaveDensity = fn::add(std::move(layerized), std::move(solidifiedCheese));
    Function subtractions = fn::min(fn::min(std::move(baseCaveDensity), std::move(entranceDensity)),
                                    fn::add(std::move(spaghetti), std::move(roughness)));
    // Bound first: argument order is unspecified, so the copy cannot share a call with the move.
    Function pillarInput = pillarSource;
    Function pillarChoice = fn::rangeChoice(std::move(pillarInput), -1'000'000.0, 0.03, fn::constant(-1'000'000.0),
                                            std::move(pillarSource));
    return fn::max(std::move(subtractions), std::move(pillarChoice));
}

} // namespace

OverworldNoiseRouter noiseRouterData::overworld(const NoiseLookup& noise, Function base3d) {
    Function barrierNoise = fn::noise(noise("aquifer_barrier"), 1.0, 0.5);
    Function fluidLevelFloodednessNoise = fn::noise(noise("aquifer_fluid_level_floodedness"), 1.0, 0.67);
    Function fluidLevelSpreadNoise = fn::noise(noise("aquifer_fluid_level_spread"), 1.0, 0.7142857142857143);
    Function lavaNoise = fn::noise(noise("aquifer_lava"));
    Function shiftX = fn::flatCache(fn::cache2d(fn::shiftA(noise("offset"))));
    Function shiftZ = fn::flatCache(fn::cache2d(fn::shiftB(noise("offset"))));

    Function continents = fn::flatCache(fn::shiftedNoise2d(shiftX, shiftZ, 0.25, noise("continentalness")));
    Function erosion = fn::flatCache(fn::shiftedNoise2d(shiftX, shiftZ, 0.25, noise("erosion")));
    Function ridge = fn::flatCache(fn::shiftedNoise2d(shiftX, shiftZ, 0.25, noise("ridge")));
    Function ridgesFolded = peaksAndValleys(ridge);
    Function jaggedNoise = fn::noise(noise("jagged"), 1500.0, 0.0);

    const Coordinate continentsCoord = fn::splineCoordinate(continents);
    const Coordinate erosionCoord = fn::splineCoordinate(erosion);
    const Coordinate weirdnessCoord = fn::splineCoordinate(ridge);
    const Coordinate ridgesCoord = fn::splineCoordinate(std::move(ridgesFolded));

    Function offsetSpline = fn::spline(terrainProvider::overworldOffset(continentsCoord, erosionCoord, ridgesCoord));
    Function offset =
        splineWithBlending(fn::add(fn::constant(kGlobalOffset), std::move(offsetSpline)), fn::blendOffset());

    Function factorSpline =
        fn::spline(terrainProvider::overworldFactor(continentsCoord, erosionCoord, weirdnessCoord, ridgesCoord));
    Function factor = splineWithBlending(std::move(factorSpline), fn::constant(10.0));

    Function depth = offsetToDepth(offset);

    Function jaggednessSpline =
        fn::spline(terrainProvider::overworldJaggedness(continentsCoord, erosionCoord, weirdnessCoord, ridgesCoord));
    Function unscaledJaggedness = splineWithBlending(std::move(jaggednessSpline), fn::zero());
    Function jaggedness =
        fn::flatCache(fn::mul(std::move(unscaledJaggedness), fn::halfNegative(std::move(jaggedNoise))));

    Function initialDensity = noiseGradientDensity(factor, fn::add(depth, std::move(jaggedness)));
    Function slopedCheese = fn::cacheOnce(fn::add(std::move(initialDensity), std::move(base3d)));

    Function roughness = spaghettiRoughness(noise);
    Function spaghetti2dThickness =
        fn::cacheOnce(fn::mappedNoise(noise("spaghetti_2d_thickness"), 2.0, 1.0, -0.6, -1.3));
    Function entranceDensity = entrances(noise, roughness);
    Function spaghetti = spaghetti2d(noise, std::move(spaghetti2dThickness));
    Function pillarSource = pillars(noise);
    Function noodleDensity = noodle(noise);
    Function surfaceWithEntrances = fn::min(slopedCheese, fn::mul(fn::constant(5.0), entranceDensity));
    Function cheeseSelector = slopedCheese;
    Function undergroundDensity =
        underground(noise, std::move(slopedCheese), std::move(spaghetti), std::move(roughness),
                    std::move(entranceDensity), std::move(pillarSource));
    Function caves = fn::rangeChoice(std::move(cheeseSelector), -1'000'000.0, 1.5625, std::move(surfaceWithEntrances),
                                     std::move(undergroundDensity));
    Function fullNoise = fn::min(postProcess(slideOverworld(std::move(caves))), std::move(noodleDensity));
    Function surfaceLevel = preliminarySurfaceLevel(std::move(offset), std::move(factor));

    Function y = fn::yClampedGradient(-4064, 4062, -4064.0, 4062.0);
    Function veinToggle = yLimitedInterpolatable(y, fn::noise(noise("ore_veininess"), 1.5, 1.5), -60, 50, 0.0);
    Function veinA = fn::abs(yLimitedInterpolatable(y, fn::noise(noise("ore_vein_a"), 4.0, 4.0), -60, 50, 0.0));
    Function veinB =
        fn::abs(yLimitedInterpolatable(std::move(y), fn::noise(noise("ore_vein_b"), 4.0, 4.0), -60, 50, 0.0));
    // parity: the source literal is a float promoted into constant(double).
    Function veinRidged =
        fn::add(fn::constant(static_cast<double>(-0.08F)), fn::max(std::move(veinA), std::move(veinB)));
    Function veinGap = fn::noise(noise("ore_gap"));

    Function temperature = fn::shiftedNoise2d(shiftX, shiftZ, 0.25, noise("temperature"));
    Function vegetation = fn::shiftedNoise2d(shiftX, shiftZ, 0.25, noise("vegetation"));

    return OverworldNoiseRouter{
        .barrierNoise = std::move(barrierNoise),
        .fluidLevelFloodednessNoise = std::move(fluidLevelFloodednessNoise),
        .fluidLevelSpreadNoise = std::move(fluidLevelSpreadNoise),
        .lavaNoise = std::move(lavaNoise),
        .shiftX = std::move(shiftX),
        .shiftZ = std::move(shiftZ),
        .temperature = std::move(temperature),
        .vegetation = std::move(vegetation),
        .continents = std::move(continents),
        .erosion = std::move(erosion),
        .depth = std::move(depth),
        .ridges = std::move(ridge),
        .preliminarySurfaceLevel = std::move(surfaceLevel),
        .finalDensity = std::move(fullNoise),
        .veinToggle = std::move(veinToggle),
        .veinRidged = std::move(veinRidged),
        .veinGap = std::move(veinGap),
    };
}

NetherNoiseRouter noiseRouterData::nether(const NoiseLookup& noise, Function base3d) {
    Function temperature = fn::shiftedNoise2d(fn::zero(), fn::zero(), 0.25, noise("nether/temperature"));
    Function vegetation = fn::shiftedNoise2d(fn::zero(), fn::zero(), 0.25, noise("nether/vegetation"));
    Function finalDensity = postProcess(slide(std::move(base3d), 0, 128, 24, 0, 0.9375, -8, 24, 2.5));
    return NetherNoiseRouter{
        .temperature = std::move(temperature),
        .vegetation = std::move(vegetation),
        .finalDensity = std::move(finalDensity),
    };
}

EndNoiseRouter noiseRouterData::end(Function base3d, std::int64_t seed) {
    Function islands = fn::endIslands(seed);
    Function slopedCheese = fn::add(islands, std::move(base3d));
    Function finalDensity = postProcess(slide(std::move(slopedCheese), 0, 128, 72, -184, -23.4375, 4, 32, -0.234375));
    return EndNoiseRouter{
        .erosion = fn::cache2d(std::move(islands)),
        .finalDensity = std::move(finalDensity),
    };
}

} // namespace basalt::server::world::terrain
