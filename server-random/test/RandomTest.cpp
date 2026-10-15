#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>

#include <doctest.h>

#include <cstdint>
#include <vector>

using namespace basalt::server::random;

namespace {
std::vector<std::int32_t> boundedInts(RandomSource& random, std::int32_t bound, int count) {
    std::vector<std::int32_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.push_back(random.nextInt(bound));
    }
    return out;
}
} // namespace

TEST_CASE("LegacyRandomSource full sequence matches vanilla for seed 0") {
    RandomSource random = RandomSource::legacy(0);
    CHECK(random.nextLong() == -4962768465676381896LL);
    CHECK(random.nextLong() == 4437113781045784766LL);
    CHECK(random.nextLong() == -6688467811848818630LL);
    CHECK(random.nextFloat() == 0.550437F);
    CHECK(random.nextFloat() == 0.1170066F);
    CHECK(random.nextFloat() == 0.59754527F);
    CHECK(random.nextDouble() == 0.7815346320453048);
    CHECK(random.nextDouble() == 0.2527761665759859);
    CHECK(random.nextDouble() == 0.6130357680446138);
    CHECK(random.nextBoolean());
    CHECK(random.nextBoolean());
    CHECK_FALSE(random.nextBoolean());
    CHECK(random.nextGaussian() == 0.17268833153399873);
    CHECK(random.nextGaussian() == -0.08807455272616774);
    CHECK(random.nextGaussian() == -0.4118293663901143);
}

TEST_CASE("LegacyRandomSource forkPositional matches vanilla") {
    RandomSource seedSource = RandomSource::legacy(42);
    const PositionalRandomFactory factory = seedSource.forkPositional();

    struct Case {
        std::int32_t x, y, z, first, second;
    };

    const std::vector<Case> cases = {
        {0, 0, 0, 2040010338, -1911889734},
        {1, 2, 3, -1062610619, 511209597},
        {-5, 64, 100, -1147523588, -461951395},
        {123, -45, 678, 1038672630, 1184260035},
        {1000000, 0, -1000000, 2082118675, -270010132},
    };
    for (const Case& c : cases) {
        RandomSource random = factory.at(c.x, c.y, c.z);
        CHECK(random.nextInt() == c.first);
        CHECK(random.nextInt() == c.second);
    }
}

TEST_CASE("XoroshiroRandomSource full sequence matches vanilla for seed 0") {
    RandomSource random = RandomSource::xoroshiro(0);
    CHECK(random.nextFloat() == 0.16474366F);
    CHECK(random.nextFloat() == 0.7997457F);
    CHECK(random.nextFloat() == 0.25119615F);
    CHECK(random.nextDouble() == 0.11712489470639631);
    CHECK(random.nextDouble() == 0.0997124786680137);
    CHECK(random.nextDouble() == 0.7566797430601416);
    CHECK_FALSE(random.nextBoolean());
    CHECK(random.nextBoolean());
    CHECK(random.nextBoolean());
    CHECK(random.nextGaussian() == 0.31104841012084117);
    CHECK(random.nextGaussian() == -1.0186152786011977);
    CHECK(random.nextGaussian() == -0.03511119974585956);
}

TEST_CASE("XoroshiroRandomSource forkPositional matches vanilla") {
    RandomSource seedSource = RandomSource::xoroshiro(42);
    const PositionalRandomFactory factory = seedSource.forkPositional();

    struct Case {
        std::int32_t x, y, z, first, second;
    };

    const std::vector<Case> cases = {
        {0, 0, 0, 89793166, -1194968252},
        {1, 2, 3, 1942892291, -1485131184},
        {-5, 64, 100, -1435539674, 262546357},
        {123, -45, 678, 1740712342, -539362039},
        {1000000, 0, -1000000, -600618616, 1099295309},
    };
    for (const Case& c : cases) {
        RandomSource random = factory.at(c.x, c.y, c.z);
        CHECK(random.nextInt() == c.first);
        CHECK(random.nextInt() == c.second);
    }
}

TEST_CASE("RandomSupport matches vanilla") {
    CHECK(mixStafford13(0) == 0LL);
    CHECK(mixStafford13(1) == 6238072747940578789LL);
    CHECK(mixStafford13(42) == -6387817139659442654LL);
    CHECK(mixStafford13(-1) == -5417735806833148549LL);
    CHECK(mixStafford13(123456789) == -1000775671988880032LL);

    CHECK(upgradeSeedTo128bit(0) == Seed128bit{3847398142028685078LL, 7192185014346937746LL});
    CHECK(upgradeSeedTo128bit(1) == Seed128bit{5272463233947570727LL, 1927618558350093866LL});
    CHECK(upgradeSeedTo128bit(42) == Seed128bit{6720814022939733433LL, -2851323883594622011LL});
    CHECK(upgradeSeedTo128bit(-1) == Seed128bit{-110783831392733308LL, 2932223646667407290LL});
    CHECK(upgradeSeedTo128bit(123456789) == Seed128bit{-7278655050202467086LL, -3669656285675084719LL});

    CHECK(seedFromHashOf("") == Seed128bit{-3162216497309240828LL, -1621285313438006658LL});
    CHECK(seedFromHashOf("minecraft:overworld") == Seed128bit{4754257565590824948LL, -1113069127075346987LL});
    CHECK(seedFromHashOf("hello") == Seed128bit{6719722671305337462LL, -5084109257958767214LL});
}

TEST_CASE("WorldgenRandom seeding matches vanilla over xoroshiro") {
    RandomSource random = RandomSource::worldgen(RandomSource::xoroshiro(999));
    CHECK(setDecorationSeed(random, 1234567, 32, -16) == -8234665531566012489LL);
    CHECK(boundedInts(random, 16, 8) == std::vector<std::int32_t>{9, 12, 2, 0, 13, 3, 8, 3});
    CHECK(random.nextFloat() == 0.49740642F);
}

TEST_CASE("WorldgenRandom seeding matches vanilla over legacy") {
    RandomSource random = RandomSource::worldgen(RandomSource::legacy(0));
    setLargeFeatureWithSalt(random, 9876543, 3, -2, 10387312);
    CHECK(boundedInts(random, 26, 6) == std::vector<std::int32_t>{0, 11, 12, 5, 4, 9});
}

TEST_CASE("WorldgenRandom decoration seed at the chunk origin matches vanilla") {
    RandomSource random = RandomSource::worldgen(RandomSource::xoroshiro(1));
    setDecorationSeed(random, -42, 0, 0);
    CHECK(boundedInts(random, 96, 4) == std::vector<std::int32_t>{46, 7, 93, 55});
}
