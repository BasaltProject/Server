#pragma once

#include <basalt/server/world/noise/NormalNoise.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// Parses one real `data/minecraft/worldgen/noise/<name>.json` entry, read from the operator's own
/// downloaded assets at runtime and never embedded here.
///
/// parity: `NormalNoise.NoiseParameters.DIRECT_CODEC`, which is
/// `{"firstOctave": int, "amplitudes": [double, ...]}` and nothing else.
namespace noiseParametersJson {

/// Nothing for a document this does not understand: an unsupported shape, not merely bad JSON.
[[nodiscard]] std::optional<server::world::noise::NormalNoise::Parameters> parse(std::string_view json);

} // namespace noiseParametersJson

} // namespace basalt::server::runtime::world
