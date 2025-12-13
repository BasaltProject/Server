#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

/// parity: `BiomeManager.obfuscateSeed`, including Guava's little-endian `hashLong`/`asLong`.
///
/// The seed goes in as eight little-endian bytes and the first eight bytes of the digest come back
/// out the same way round, which is what Guava's `HashCode.asLong` does. Getting the order wrong
/// gives a world whose biomes are subtly the wrong shape rather than an error.
[[nodiscard]] std::int64_t obfuscateBiomeSeed(std::int64_t seed);

} // namespace basalt::server::runtime::world
