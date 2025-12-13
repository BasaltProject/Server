#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>

#include <doctest.h>

using basalt::server::runtime::world::obfuscateBiomeSeed;

TEST_CASE("the biome zoom seed is the digest read back little-endian") {
    // Derived with `sha256` over the seed's own eight little-endian bytes, the first eight of the
    // digest read back the same way round: `python3 -c "..."`, not remembered.
    CHECK(obfuscateBiomeSeed(0) == 8794265229978523055LL);
    CHECK(obfuscateBiomeSeed(1234) == 7029374744722207258LL);
    CHECK(obfuscateBiomeSeed(-1) == 6759447113877070610LL);
}

TEST_CASE("two seeds a step apart obfuscate to nothing like each other") {
    // The whole point of the hash: neighbouring worlds must not share a biome layout.
    CHECK(obfuscateBiomeSeed(0) != obfuscateBiomeSeed(1));
    CHECK(obfuscateBiomeSeed(1234) != obfuscateBiomeSeed(1235));
}
