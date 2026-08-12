package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.densityfunction.EndIslandDensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.densityfunction.SplineCoordinate
import org.basaltmc.server.world.noise.NormalNoise

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseRouterData, the `overworld` assembly only.
 * The graph shape is code, not datapack content: only the named noise parameters in [noise] come
 * from `data/minecraft/worldgen/noise/`, loaded at runtime and never embedded here.
 *
 * Gap: only the non-large-biomes non-amplified variant.
 */
public class OverworldNoiseRouter(
    public val barrierNoise: DensityFunction,
    public val fluidLevelFloodednessNoise: DensityFunction,
    public val fluidLevelSpreadNoise: DensityFunction,
    public val lavaNoise: DensityFunction,
    public val shiftX: DensityFunction,
    public val shiftZ: DensityFunction,
    public val temperature: DensityFunction,
    public val vegetation: DensityFunction,
    public val continents: DensityFunction,
    public val erosion: DensityFunction,
    public val depth: DensityFunction,
    public val ridges: DensityFunction,
    public val preliminarySurfaceLevel: DensityFunction,
    public val finalDensity: DensityFunction,
    public val veinToggle: DensityFunction,
    public val veinRidged: DensityFunction,
    public val veinGap: DensityFunction,
)

/** The three non-zero functions in vanilla's Nether router. */
public class NetherNoiseRouter(
    public val temperature: DensityFunction,
    public val vegetation: DensityFunction,
    public val finalDensity: DensityFunction,
)

/** The two non-zero functions in vanilla's End router. */
public class EndNoiseRouter(
    public val erosion: DensityFunction,
    public val finalDensity: DensityFunction,
)

public object NoiseRouterData {
    // parity: NoiseRouterData.GLOBAL_OFFSET.
    // The source literal is a float promoted into DensityFunctions.constant(double).
    private const val GLOBAL_OFFSET = -0.5037500262260437
    private val BLENDING_FACTOR = DensityFunctions.constant(10.0)
    private val BLENDING_JAGGEDNESS = DensityFunctions.zero()

    public fun overworld(
        noise: (id: String) -> NormalNoise,
        base3d: DensityFunction,
    ): OverworldNoiseRouter {
        val barrierNoise = DensityFunctions.noise(noise("aquifer_barrier"), 1.0, 0.5)
        val fluidLevelFloodednessNoise = DensityFunctions.noise(noise("aquifer_fluid_level_floodedness"), 1.0, 0.67)
        val fluidLevelSpreadNoise = DensityFunctions.noise(noise("aquifer_fluid_level_spread"), 1.0, 0.7142857142857143)
        val lavaNoise = DensityFunctions.noise(noise("aquifer_lava"))
        val shiftX = DensityFunctions.flatCache(DensityFunctions.cache2d(DensityFunctions.shiftA(noise("offset"))))
        val shiftZ = DensityFunctions.flatCache(DensityFunctions.cache2d(DensityFunctions.shiftB(noise("offset"))))

        val continents =
            DensityFunctions.flatCache(
                DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("continentalness")),
            )
        val erosion =
            DensityFunctions.flatCache(
                DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("erosion")),
            )
        val ridge = DensityFunctions.flatCache(DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("ridge")))
        val ridgesFolded = peaksAndValleys(ridge)
        val jaggedNoise = DensityFunctions.noise(noise("jagged"), 1500.0, 0.0)

        val continentsCoord = SplineCoordinate(continents)
        val erosionCoord = SplineCoordinate(erosion)
        val weirdnessCoord = SplineCoordinate(ridge)
        val ridgesCoord = SplineCoordinate(ridgesFolded)

        val offsetSpline =
            DensityFunctions.spline(
                TerrainProvider.overworldOffset<FunctionContext, SplineCoordinate>(
                    continentsCoord,
                    erosionCoord,
                    ridgesCoord,
                ),
            )
        val offset =
            splineWithBlending(
                DensityFunctions.add(DensityFunctions.constant(GLOBAL_OFFSET), offsetSpline),
                DensityFunctions.blendOffset(),
            )

        val factorSpline =
            DensityFunctions.spline(
                TerrainProvider.overworldFactor<FunctionContext, SplineCoordinate>(
                    continentsCoord,
                    erosionCoord,
                    weirdnessCoord,
                    ridgesCoord,
                ),
            )
        val factor = splineWithBlending(factorSpline, BLENDING_FACTOR)

        val depth = offsetToDepth(offset)

        val jaggednessSpline =
            DensityFunctions.spline(
                TerrainProvider.overworldJaggedness<FunctionContext, SplineCoordinate>(
                    continentsCoord,
                    erosionCoord,
                    weirdnessCoord,
                    ridgesCoord,
                ),
            )
        val unscaledJaggedness = splineWithBlending(jaggednessSpline, BLENDING_JAGGEDNESS)
        val jaggedness =
            DensityFunctions.flatCache(
                DensityFunctions.mul(unscaledJaggedness, DensityFunctions.halfNegative(jaggedNoise)),
            )

        val initialDensity = noiseGradientDensity(factor, DensityFunctions.add(depth, jaggedness))
        val slopedCheese = DensityFunctions.cacheOnce(DensityFunctions.add(initialDensity, base3d))

        val spaghettiRoughness = spaghettiRoughness(noise)
        val spaghetti2dThickness =
            DensityFunctions.cacheOnce(
                DensityFunctions.mappedNoise(noise("spaghetti_2d_thickness"), 2.0, 1.0, -0.6, -1.3),
            )
        val entrances = entrances(noise, spaghettiRoughness)
        val spaghetti2d = spaghetti2d(noise, spaghetti2dThickness)
        val pillars = pillars(noise)
        val noodle = noodle(noise)
        val surfaceWithEntrances =
            DensityFunctions.min(
                slopedCheese,
                DensityFunctions.mul(DensityFunctions.constant(5.0), entrances),
            )
        val caves =
            DensityFunctions.rangeChoice(
                slopedCheese,
                -1_000_000.0,
                1.5625,
                surfaceWithEntrances,
                underground(noise, slopedCheese, spaghetti2d, spaghettiRoughness, entrances, pillars),
            )
        val fullNoise = DensityFunctions.min(postProcess(slideOverworld(caves)), noodle)
        val preliminarySurfaceLevel = preliminarySurfaceLevel(offset, factor)
        val y = DensityFunctions.yClampedGradient(-4064, 4062, -4064.0, 4062.0)
        val veinToggle =
            yLimitedInterpolatable(y, DensityFunctions.noise(noise("ore_veininess"), 1.5, 1.5), -60, 50, 0.0)
        val veinA =
            DensityFunctions.abs(
                yLimitedInterpolatable(y, DensityFunctions.noise(noise("ore_vein_a"), 4.0, 4.0), -60, 50, 0.0),
            )
        val veinB =
            DensityFunctions.abs(
                yLimitedInterpolatable(y, DensityFunctions.noise(noise("ore_vein_b"), 4.0, 4.0), -60, 50, 0.0),
            )
        val veinRidged =
            DensityFunctions.add(
                DensityFunctions.constant((-0.08f).toDouble()),
                DensityFunctions.max(veinA, veinB),
            )
        val veinGap = DensityFunctions.noise(noise("ore_gap"))

        val temperature = DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("temperature"))
        val vegetation = DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("vegetation"))

        return OverworldNoiseRouter(
            barrierNoise = barrierNoise,
            fluidLevelFloodednessNoise = fluidLevelFloodednessNoise,
            fluidLevelSpreadNoise = fluidLevelSpreadNoise,
            lavaNoise = lavaNoise,
            shiftX = shiftX,
            shiftZ = shiftZ,
            temperature = temperature,
            vegetation = vegetation,
            continents = continents,
            erosion = erosion,
            depth = depth,
            ridges = ridge,
            preliminarySurfaceLevel = preliminarySurfaceLevel,
            finalDensity = fullNoise,
            veinToggle = veinToggle,
            veinRidged = veinRidged,
            veinGap = veinGap,
        )
    }

    /** // parity: `NoiseRouterData.nether`, using `noise_settings/nether.json`. */
    public fun nether(
        noise: (id: String) -> NormalNoise,
        base3d: DensityFunction,
    ): NetherNoiseRouter {
        val zero = DensityFunctions.zero()
        val temperature = DensityFunctions.shiftedNoise2d(zero, zero, 0.25, noise("nether/temperature"))
        val vegetation =
            DensityFunctions.shiftedNoise2d(
                DensityFunctions.zero(),
                DensityFunctions.zero(),
                0.25,
                noise("nether/vegetation"),
            )
        val finalDensity = postProcess(slide(base3d, 0, 128, 24, 0, 0.9375, -8, 24, 2.5))
        return NetherNoiseRouter(temperature, vegetation, finalDensity)
    }

    /** // parity: `NoiseRouterData.end`, using `noise_settings/end.json`. */
    public fun end(
        base3d: DensityFunction,
        seed: Long,
    ): EndNoiseRouter {
        val islands = EndIslandDensityFunction(seed)
        val slopedCheese = DensityFunctions.add(islands, base3d)
        val finalDensity = postProcess(slide(slopedCheese, 0, 128, 72, -184, -23.4375, 4, 32, -0.234375))
        return EndNoiseRouter(DensityFunctions.cache2d(islands), finalDensity)
    }

    // parity: NoiseRouterData.peaksAndValleys(DensityFunction).
    private fun peaksAndValleys(weirdness: DensityFunction): DensityFunction =
        DensityFunctions.mul(
            DensityFunctions.add(
                DensityFunctions.abs(
                    DensityFunctions.add(
                        DensityFunctions.abs(weirdness),
                        DensityFunctions.constant(-0.6666666666666666),
                    ),
                ),
                DensityFunctions.constant(-0.3333333333333333),
            ),
            DensityFunctions.constant(-3.0),
        )

    // parity: NoiseRouterData.offsetToDepth(DensityFunction).
    private fun offsetToDepth(offset: DensityFunction): DensityFunction =
        DensityFunctions.add(DensityFunctions.yClampedGradient(-64, 320, 1.5, -1.5), offset)

    // parity: NoiseRouterData.splineWithBlending(DensityFunction, DensityFunction).
    private fun splineWithBlending(
        spline: DensityFunction,
        blendingTarget: DensityFunction,
    ): DensityFunction {
        val blendedSpline = DensityFunctions.lerp(DensityFunctions.blendAlpha(), blendingTarget, spline)
        return DensityFunctions.flatCache(DensityFunctions.cache2d(blendedSpline))
    }

    // parity: NoiseRouterData.noiseGradientDensity(DensityFunction, DensityFunction).
    private fun noiseGradientDensity(
        factor: DensityFunction,
        depthWithJaggedness: DensityFunction,
    ): DensityFunction {
        val gradientUnscaled = DensityFunctions.mul(depthWithJaggedness, factor)
        return DensityFunctions.mul(DensityFunctions.constant(4.0), DensityFunctions.quarterNegative(gradientUnscaled))
    }

    // parity: NoiseRouterData.preliminarySurfaceLevel, non-amplified Overworld.
    private fun preliminarySurfaceLevel(
        offset: DensityFunction,
        factor: DensityFunction,
    ): DensityFunction {
        val cachedFactor = DensityFunctions.cache2d(factor)
        val cachedOffset = DensityFunctions.cache2d(offset)
        val upperBound =
            DensityFunctions.clamp(
                remap(
                    DensityFunctions.add(
                        DensityFunctions.mul(
                            DensityFunctions.constant(0.2734375),
                            DensityFunctions.invert(cachedFactor),
                        ),
                        DensityFunctions.mul(DensityFunctions.constant(-1.0), cachedOffset),
                    ),
                    1.5,
                    -1.5,
                    -64.0,
                    320.0,
                ),
                -40.0,
                320.0,
            )
        val density =
            DensityFunctions.add(
                slideOverworld(
                    DensityFunctions.clamp(
                        DensityFunctions.add(
                            noiseGradientDensity(cachedFactor, offsetToDepth(cachedOffset)),
                            DensityFunctions.constant(-0.703125),
                        ),
                        -64.0,
                        64.0,
                    ),
                ),
                DensityFunctions.constant(-0.390625),
            )
        return DensityFunctions.findTopSurface(density, upperBound, -64, 8)
    }

    private fun remap(
        input: DensityFunction,
        fromMin: Double,
        fromMax: Double,
        toMin: Double,
        toMax: Double,
    ): DensityFunction {
        val factor = (toMax - toMin) / (fromMax - fromMin)
        val offset = toMin - fromMin * factor
        return DensityFunctions.add(
            DensityFunctions.mul(input, DensityFunctions.constant(factor)),
            DensityFunctions.constant(offset),
        )
    }

    // parity: NoiseRouterData.spaghettiRoughnessFunction.
    private fun spaghettiRoughness(noise: (id: String) -> NormalNoise): DensityFunction {
        val roughness = DensityFunctions.noise(noise("spaghetti_roughness"))
        val modulator =
            DensityFunctions.mappedNoise(
                noise("spaghetti_roughness_modulator"),
                minTarget = 0.0,
                maxTarget = -0.1,
            )
        return DensityFunctions.cacheOnce(
            DensityFunctions.mul(
                modulator,
                DensityFunctions.add(DensityFunctions.abs(roughness), DensityFunctions.constant(-0.4)),
            ),
        )
    }

    // parity: NoiseRouterData.entrances.
    private fun entrances(
        noise: (id: String) -> NormalNoise,
        spaghettiRoughness: DensityFunction,
    ): DensityFunction {
        val rarity = DensityFunctions.cacheOnce(DensityFunctions.noise(noise("spaghetti_3d_rarity"), 2.0, 1.0))
        val thickness =
            DensityFunctions.mappedNoise(
                noise("spaghetti_3d_thickness"),
                minTarget = -0.065,
                maxTarget = -0.088,
            )
        val cave1 = wrapSpaghettiRarity3d(rarity, noise("spaghetti_3d_1"))
        val cave2 = wrapSpaghettiRarity3d(rarity, noise("spaghetti_3d_2"))
        val spaghetti =
            DensityFunctions.clamp(
                DensityFunctions.add(DensityFunctions.max(cave1, cave2), thickness),
                -1.0,
                1.0,
            )
        val largeEntrance =
            DensityFunctions.add(
                DensityFunctions.add(
                    DensityFunctions.noise(noise("cave_entrance"), 0.75, 0.5),
                    DensityFunctions.constant(0.37),
                ),
                DensityFunctions.yClampedGradient(-10, 30, 0.3, 0.0),
            )
        return DensityFunctions.cacheOnce(
            DensityFunctions.min(largeEntrance, DensityFunctions.add(spaghettiRoughness, spaghetti)),
        )
    }

    // parity: NoiseRouterData.spaghetti2D.
    private fun spaghetti2d(
        noise: (id: String) -> NormalNoise,
        thickness: DensityFunction,
    ): DensityFunction {
        val rarity = DensityFunctions.noise(noise("spaghetti_2d_modulator"), 2.0, 1.0)
        val cave = wrapSpaghettiRarity2d(rarity, noise("spaghetti_2d"))
        val elevation =
            DensityFunctions.mappedNoise(
                noise("spaghetti_2d_elevation"),
                xzScale = 1.0,
                yScale = 0.0,
                minTarget = -8.0,
                maxTarget = 8.0,
            )
        val sloped =
            DensityFunctions.abs(
                DensityFunctions.add(
                    DensityFunctions.flatCache(elevation),
                    DensityFunctions.yClampedGradient(-64, 320, 8.0, -40.0),
                ),
            )
        val layerRidged = DensityFunctions.cube(DensityFunctions.add(sloped, thickness))
        val caveNoise =
            DensityFunctions.add(
                cave,
                DensityFunctions.mul(DensityFunctions.constant(0.083), thickness),
            )
        return DensityFunctions.clamp(DensityFunctions.max(caveNoise, layerRidged), -1.0, 1.0)
    }

    // parity: NoiseRouterData.pillars.
    private fun pillars(noise: (id: String) -> NormalNoise): DensityFunction {
        val source = DensityFunctions.noise(noise("pillar"), 25.0, 0.3)
        val rareness = DensityFunctions.mappedNoise(noise("pillar_rareness"), minTarget = 0.0, maxTarget = -2.0)
        val thickness = DensityFunctions.mappedNoise(noise("pillar_thickness"), minTarget = 0.0, maxTarget = 1.1)
        val withRareness =
            DensityFunctions.add(
                DensityFunctions.mul(source, DensityFunctions.constant(2.0)),
                rareness,
            )
        return DensityFunctions.cacheOnce(DensityFunctions.mul(withRareness, DensityFunctions.cube(thickness)))
    }

    // parity: NoiseRouterData.noodle.
    private fun noodle(noise: (id: String) -> NormalNoise): DensityFunction {
        val y = DensityFunctions.yClampedGradient(-4064, 4062, -4064.0, 4062.0)
        val toggle =
            yLimitedInterpolatable(y, DensityFunctions.noise(noise("noodle")), -60, 320, -1.0)
        val thickness =
            yLimitedInterpolatable(
                y,
                DensityFunctions.mappedNoise(
                    noise("noodle_thickness"),
                    minTarget = -0.05,
                    maxTarget = -0.1,
                ),
                -60,
                320,
                0.0,
            )
        val ridgeA =
            yLimitedInterpolatable(
                y,
                DensityFunctions.noise(noise("noodle_ridge_a"), 2.6666666666666665, 2.6666666666666665),
                -60,
                320,
                0.0,
            )
        val ridgeB =
            yLimitedInterpolatable(
                y,
                DensityFunctions.noise(noise("noodle_ridge_b"), 2.6666666666666665, 2.6666666666666665),
                -60,
                320,
                0.0,
            )
        val ridged =
            DensityFunctions.mul(
                DensityFunctions.constant(1.5),
                DensityFunctions.max(DensityFunctions.abs(ridgeA), DensityFunctions.abs(ridgeB)),
            )
        return DensityFunctions.rangeChoice(
            toggle,
            -1_000_000.0,
            0.0,
            DensityFunctions.constant(64.0),
            DensityFunctions.add(thickness, ridged),
        )
    }

    // parity: NoiseRouterData.underground.
    private fun underground(
        noise: (id: String) -> NormalNoise,
        slopedCheese: DensityFunction,
        spaghetti2d: DensityFunction,
        spaghettiRoughness: DensityFunction,
        entrances: DensityFunction,
        pillarSource: DensityFunction,
    ): DensityFunction {
        val layer = DensityFunctions.noise(noise("cave_layer"), xzScale = 1.0, yScale = 8.0)
        val layerized = DensityFunctions.mul(DensityFunctions.constant(4.0), DensityFunctions.square(layer))
        val cheese = DensityFunctions.noise(noise("cave_cheese"), xzScale = 1.0, yScale = 0.6666666666666666)
        val solidifiedCheese =
            DensityFunctions.add(
                DensityFunctions.clamp(
                    DensityFunctions.add(DensityFunctions.constant(0.27), cheese),
                    -1.0,
                    1.0,
                ),
                DensityFunctions.clamp(
                    DensityFunctions.add(
                        DensityFunctions.constant(1.5),
                        DensityFunctions.mul(DensityFunctions.constant(-0.64), slopedCheese),
                    ),
                    0.0,
                    0.5,
                ),
            )
        val baseCaveDensity = DensityFunctions.add(layerized, solidifiedCheese)
        val subtractions =
            DensityFunctions.min(
                DensityFunctions.min(baseCaveDensity, entrances),
                DensityFunctions.add(spaghetti2d, spaghettiRoughness),
            )
        val pillars =
            DensityFunctions.rangeChoice(
                pillarSource,
                -1_000_000.0,
                0.03,
                DensityFunctions.constant(-1_000_000.0),
                pillarSource,
            )
        return DensityFunctions.max(subtractions, pillars)
    }

    private fun yLimitedInterpolatable(
        y: DensityFunction,
        whenInRange: DensityFunction,
        minYInclusive: Int,
        maxYInclusive: Int,
        whenOutOfRange: Double,
    ): DensityFunction =
        DensityFunctions.interpolated(
            DensityFunctions.rangeChoice(
                y,
                minYInclusive.toDouble(),
                (maxYInclusive + 1).toDouble(),
                whenInRange,
                DensityFunctions.constant(whenOutOfRange),
            ),
        )

    private fun wrapSpaghettiRarity2d(
        rarity: DensityFunction,
        targetNoise: NormalNoise,
    ): DensityFunction =
        wrapSpaghettiRarity(
            rarity,
            targetNoise,
            doubleArrayOf(-0.75, -0.5, 0.5, 0.75),
            doubleArrayOf(0.5, 0.75, 1.0, 2.0, 3.0),
        )

    private fun wrapSpaghettiRarity3d(
        rarity: DensityFunction,
        targetNoise: NormalNoise,
    ): DensityFunction =
        wrapSpaghettiRarity(
            rarity,
            targetNoise,
            doubleArrayOf(-0.5, 0.0, 0.5),
            doubleArrayOf(0.75, 1.0, 1.5, 2.0),
        )

    // parity: NoiseRouterData.QuantizedSpaghettiRarity.
    private fun wrapSpaghettiRarity(
        input: DensityFunction,
        targetNoise: NormalNoise,
        thresholds: DoubleArray,
        rarities: DoubleArray,
    ): DensityFunction =
        DensityFunctions.abs(
            DensityFunctions.intervalSelect(
                input,
                thresholds,
                rarities.map { rarity ->
                    DensityFunctions.mul(
                        DensityFunctions.constant(rarity),
                        DensityFunctions.noise(targetNoise, 1.0 / rarity, 1.0 / rarity),
                    )
                },
            ),
        )

    // parity: NoiseRouterData.postProcess(DensityFunction).
    private fun postProcess(slide: DensityFunction): DensityFunction {
        val blended = DensityFunctions.blendDensity(slide)
        return DensityFunctions.squeeze(
            DensityFunctions.interpolated(DensityFunctions.mul(blended, DensityFunctions.constant(0.64))),
        )
    }

    // parity: NoiseRouterData.slideOverworld(boolean, DensityFunction). Non-amplified only.
    private fun slideOverworld(caves: DensityFunction): DensityFunction =
        slide(caves, -64, 384, 80, 64, -0.078125, 0, 24, 0.1171875)

    // parity: NoiseRouterData.slide(DensityFunction, int, int, int, int, double, int, int, double).
    private fun slide(
        caves: DensityFunction,
        minY: Int,
        height: Int,
        topStartY: Int,
        topEndY: Int,
        topTarget: Double,
        bottomStartY: Int,
        bottomEndY: Int,
        bottomTarget: Double,
    ): DensityFunction {
        val topFactor = DensityFunctions.yClampedGradient(minY + height - topStartY, minY + height - topEndY, 1.0, 0.0)
        val noiseValue = DensityFunctions.lerp(topFactor, topTarget, caves)
        val bottomFactor = DensityFunctions.yClampedGradient(minY + bottomStartY, minY + bottomEndY, 0.0, 1.0)
        return DensityFunctions.lerp(bottomFactor, bottomTarget, noiseValue)
    }
}
