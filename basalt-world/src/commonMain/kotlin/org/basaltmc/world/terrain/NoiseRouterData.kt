package org.basaltmc.world.terrain

import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.DensityFunctions
import org.basaltmc.world.densityfunction.EndIslandDensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.densityfunction.SplineCoordinate
import org.basaltmc.world.noise.NormalNoise

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseRouterData (the `overworld` assembly only,
 * plus the handful of private helpers it needs). Builds the real Overworld density-function graph
 * by combining [DensityFunctions] factory calls exactly like the real jar does: this class lives
 * in the runtime package (`world.level.levelgen`), not a datapack (`data.worldgen`), and its own
 * bootstrap calls straight into it with resolved [NormalNoise.NoiseParameters] holders, so the
 * graph *shape* below is algorithm/code, matching this project's own precedent for
 * [org.basaltmc.world.densityfunction.BlendedNoise]/[TerrainProvider]. Only the named noise
 * parameters [overworld] takes in [noise] are real datapack content (json files under
 * `data/minecraft/worldgen/noise/`), loaded elsewhere at runtime, never embedded here.
 *
 * **Deliberately narrowed scope, documented gaps:**
 * - Caves, aquifers and ore veins are not modeled: real vanilla's `fullNoise` is
 *   `min(postProcess(slideOverworld(caves)), NOODLE)` where `caves` itself folds in cave
 *   entrances/spaghetti/underground carving; this only builds the surface-relevant
 *   `slopedCheese` term (offset/factor/jaggedness/depth/base3d) and skips straight to
 *   `postProcess(slideOverworld(slopedCheese))`, the same "no caves" gap
 *   `org.basaltmc.world.NoiseWorldGenerator` already documents.
 * - Only the non-large-biomes, non-amplified variant is built (matches [TerrainProvider]'s own
 *   narrowed scope).
 * - `preliminarySurfaceLevel` (an approximate 2D surface-height estimate real vanilla's
 *   `SurfaceRules`/aquifer code queries for performance) is not built yet: nothing in this
 *   project needs it until real `SurfaceRules` do.
 */
public class OverworldNoiseRouter(
    public val shiftX: DensityFunction,
    public val shiftZ: DensityFunction,
    public val temperature: DensityFunction,
    public val vegetation: DensityFunction,
    public val continents: DensityFunction,
    public val erosion: DensityFunction,
    public val depth: DensityFunction,
    public val ridges: DensityFunction,
    public val finalDensity: DensityFunction,
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
    private const val GLOBAL_OFFSET = -0.50375
    private val BLENDING_FACTOR = DensityFunctions.constant(10.0)
    private val BLENDING_JAGGEDNESS = DensityFunctions.zero()

    public fun overworld(
        noise: (id: String) -> NormalNoise,
        base3d: DensityFunction,
    ): OverworldNoiseRouter {
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
        val slopedCheese = DensityFunctions.add(initialDensity, base3d)

        val fullNoise = postProcess(slideOverworld(slopedCheese))

        val temperature = DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("temperature"))
        val vegetation = DensityFunctions.shiftedNoise2d(shiftX, shiftZ, 0.25, noise("vegetation"))

        return OverworldNoiseRouter(
            shiftX = shiftX,
            shiftZ = shiftZ,
            temperature = temperature,
            vegetation = vegetation,
            continents = continents,
            erosion = erosion,
            depth = depth,
            ridges = ridge,
            finalDensity = fullNoise,
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
