#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `AbstractHugeMushroomFeature` plus `HugeBrownMushroomFeature` and
/// `HugeRedMushroomFeature`. Cap and stem blocks keep their exact six visible-face properties.
namespace hugeMushroomFeature {

enum class Kind : std::uint8_t { Brown, Red };

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
           Kind kind);

} // namespace hugeMushroomFeature

} // namespace basalt::server::world::terrain::decoration
