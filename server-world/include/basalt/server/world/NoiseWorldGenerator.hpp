#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world {

/// A noise-driven stepping stone, deliberately **not vanilla parity**: solid where
/// `base3dNoise + depthTerm` is positive, with vanilla's own `BlendedNoise` overworld constants.
///
/// Gaps: this project's own 2D height field rather than the real climate parameters and splines, no
/// caves, ores or aquifers, fixed-depth surface layers, a fixed seed, and trees hashed per chunk.
/// The 3D noise is sampled per cell and skipped where the depth term alone already decides the
/// sign: per-block sampling measured several seconds per chunk.
namespace noiseWorldGenerator {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ);

} // namespace noiseWorldGenerator

} // namespace basalt::server::world
