#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/spline/CubicSpline.hpp>

namespace basalt::server::world::terrain {

using Coordinate = spline::BoundedFloatFunction<densityfunction::FunctionContext>;
using TerrainSpline = spline::SplineRef<densityfunction::FunctionContext>;

/// parity: `net.minecraft.data.worldgen.TerrainProvider`, the overworld offset, factor and
/// jaggedness splines.
///
/// The control points are written into that class in the jar rather than into datapack JSON, the
/// same category of material as the blended noise. Only the non-amplified path is ported: the
/// Amplified preset's alternate value transformers are a documented gap, so the flag vanilla threads
/// through every method is dropped rather than carried unused.
namespace terrainProvider {

[[nodiscard]] TerrainSpline overworldOffset(const Coordinate& continents, const Coordinate& erosion,
                                            const Coordinate& ridges);

[[nodiscard]] TerrainSpline overworldFactor(const Coordinate& continents, const Coordinate& erosion,
                                            const Coordinate& weirdness, const Coordinate& ridges);

[[nodiscard]] TerrainSpline overworldJaggedness(const Coordinate& continents, const Coordinate& erosion,
                                                const Coordinate& weirdness, const Coordinate& ridges);

/// parity: `TerrainProvider.peaksAndValleys(float)`.
[[nodiscard]] float peaksAndValleys(float weirdness);

[[nodiscard]] TerrainSpline buildErosionOffsetSpline(const Coordinate& erosion, const Coordinate& ridges,
                                                     float lowValley, float hill, float tallHill, float mountainFactor,
                                                     float plain, float swamp, bool includeExtremeHills, bool saddle);

} // namespace terrainProvider

} // namespace basalt::server::world::terrain
