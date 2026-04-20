#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/BiomeZoomSeed.hpp>

#include <doctest.h>

using basalt::server::runtime::world::obfuscateBiomeSeed;

TEST_CASE("the biome zoom seed is the digest read back little-endian") {
    CHECK(obfuscateBiomeSeed(0) == 8794265229978523055LL);
    CHECK(obfuscateBiomeSeed(1234) == 7029374744722207258LL);
    CHECK(obfuscateBiomeSeed(-1) == 6759447113877070610LL);
}

TEST_CASE("two seeds a step apart obfuscate to nothing like each other") {
    CHECK(obfuscateBiomeSeed(0) != obfuscateBiomeSeed(1));
    CHECK(obfuscateBiomeSeed(1234) != obfuscateBiomeSeed(1235));
}
