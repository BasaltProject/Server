#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/random/WorldgenSeeds.hpp>

#include <doctest.h>

#include <cstdint>
#include <vector>

// Golden vectors captured by running the actual decompiled 26.2 server bytecode through a small JVM
// harness, not by re-deriving the algorithms from memory. They are the reason this module can be
// rewritten at all: they say what vanilla does, independently of who implements it.
using namespace basalt::server::random;

namespace {

std::vector<std::int32_t> ints(RandomSource& random, int count) {
    std::vector<std::int32_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.push_back(random.nextInt());
    }
    return out;
}

std::vector<std::int64_t> longs(RandomSource& random, int count) {
    std::vector<std::int64_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.push_back(random.nextLong());
    }
    return out;
}

std::vector<std::int32_t> boundedInts(RandomSource& random, std::int32_t bound, int count) {
    std::vector<std::int32_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        out.push_back(random.nextInt(bound));
    }
    return out;
}

std::vector<std::int32_t> legacyInts(std::int64_t seed, int count) {
    RandomSource random = RandomSource::legacy(seed);
    return ints(random, count);
}

std::vector<std::int32_t> legacyBounded(std::int64_t seed, std::int32_t bound, int count) {
    RandomSource random = RandomSource::legacy(seed);
    return boundedInts(random, bound, count);
}

std::vector<std::int32_t> xoroshiroInts(std::int64_t seed, int count) {
    RandomSource random = RandomSource::xoroshiro(seed);
    return ints(random, count);
}

std::vector<std::int64_t> xoroshiroLongs(std::int64_t seed, int count) {
    RandomSource random = RandomSource::xoroshiro(seed);
    return longs(random, count);
}

std::vector<std::int32_t> xoroshiroBounded(std::int64_t seed, std::int32_t bound, int count) {
    RandomSource random = RandomSource::xoroshiro(seed);
    return boundedInts(random, bound, count);
}

} // namespace

TEST_CASE("LegacyRandomSource nextInt matches vanilla") {
    CHECK(legacyInts(0, 5) == std::vector<std::int32_t>{-1155484576, -723955400, 1033096058, -1690734402, -1557280266});
    CHECK(legacyInts(1, 5) == std::vector<std::int32_t>{-1155869325, 431529176, 1761283695, 1749940626, 892128508});
    CHECK(legacyInts(42, 5) == std::vector<std::int32_t>{-1170105035, 234785527, -1360544799, 205897768, 1325939940});
    CHECK(legacyInts(-1, 5) == std::vector<std::int32_t>{1155099827, 1887904451, 52699159, -1941176418, -1451336087});
    CHECK(legacyInts(123456789, 5) ==
          std::vector<std::int32_t>{-1442945365, -1016548095, 1962592967, 1094656688, 1677212580});
}

TEST_CASE("LegacyRandomSource bounded nextInt matches vanilla") {
    CHECK(legacyBounded(0, 1, 5) == std::vector<std::int32_t>{0, 0, 0, 0, 0});
    CHECK(legacyBounded(0, 16, 5) == std::vector<std::int32_t>{11, 13, 3, 9, 10});
    CHECK(legacyBounded(0, 100, 5) == std::vector<std::int32_t>{60, 48, 29, 47, 15});
    CHECK(legacyBounded(0, 7, 5) == std::vector<std::int32_t>{5, 2, 4, 2, 4});
    CHECK(legacyBounded(42, 16, 5) == std::vector<std::int32_t>{11, 0, 10, 0, 4});
    CHECK(legacyBounded(42, 100, 5) == std::vector<std::int32_t>{30, 63, 48, 84, 70});
    CHECK(legacyBounded(42, 7, 5) == std::vector<std::int32_t>{1, 5, 6, 3, 5});
}

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

TEST_CASE("LegacyRandomSource full sequence matches vanilla for seed 42") {
    RandomSource random = RandomSource::legacy(42);
    CHECK(random.nextLong() == -5025562857975149833LL);
    CHECK(random.nextLong() == -5843495416241995736LL);
    CHECK(random.nextLong() == 5694868678511409995LL);
    CHECK(random.nextFloat() == 0.27707845F);
    CHECK(random.nextFloat() == 0.70771056F);
    CHECK(random.nextFloat() == 0.6655489F);
    CHECK(random.nextDouble() == 0.09132461105841794);
    CHECK(random.nextDouble() == 0.4512572194454507);
    CHECK(random.nextDouble() == 0.3816430907807956);
    CHECK(random.nextBoolean());
    CHECK_FALSE(random.nextBoolean());
    CHECK(random.nextBoolean());
    CHECK(random.nextGaussian() == -0.40086993610549193);
    CHECK(random.nextGaussian() == 1.577411619884327);
    CHECK(random.nextGaussian() == -0.4088580666890422);
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

TEST_CASE("XoroshiroRandomSource nextInt matches vanilla") {
    CHECK(xoroshiroInts(0, 5) == std::vector<std::int32_t>{-160476802, 781697906, 653572596, 1337520923, -505875771});
    CHECK(xoroshiroInts(1, 5) == std::vector<std::int32_t>{1734564350, 836234122, 825264738, -1425890201, 767430484});
    CHECK(xoroshiroInts(42, 5) ==
          std::vector<std::int32_t>{1774573855, 1357444267, -610653507, 2070806918, -1441753141});
    CHECK(xoroshiroInts(-1, 5) == std::vector<std::int32_t>{-1451817517, 2009620260, 916420186, -51837615, 933551611});
    CHECK(xoroshiroInts(123456789, 5) ==
          std::vector<std::int32_t>{1822098705, 1667009048, 231980577, -1919295214, -20373141});
}

TEST_CASE("XoroshiroRandomSource nextLong matches vanilla") {
    CHECK(xoroshiroLongs(0, 5) == std::vector<std::int64_t>{3038984756725240190LL, -3694039286755638414LL,
                                                            4633751808701151732LL, 2160572957309072155LL,
                                                            1839370574944072389LL});
    CHECK(xoroshiroLongs(1, 5) == std::vector<std::int64_t>{-1033667707219518978LL, 6451672561743293322LL,
                                                            -1821890263888393630LL, 890086654470169703LL,
                                                            8094835630745194324LL});
    CHECK(xoroshiroLongs(42, 5) == std::vector<std::int64_t>{-4695948378737616609LL, 7341713790291473579LL,
                                                             -7542733514721318211LL, 4888889476139319686LL,
                                                             8419651034331256779LL});
    CHECK(xoroshiroLongs(-1, 5) == std::vector<std::int64_t>{-8676505878415342125LL, -868585888688873692LL,
                                                             -6331679347063163302LL, -2068491455652362927LL,
                                                             -5626054917968568837LL});
    CHECK(xoroshiroLongs(123456789, 5) == std::vector<std::int64_t>{3219654894476264721LL, 8777179534091651608LL,
                                                                    6434035011650043425LL, -8486945822236614382LL,
                                                                    3449493904201163115LL});
}

TEST_CASE("XoroshiroRandomSource bounded nextInt matches vanilla") {
    CHECK(xoroshiroBounded(0, 1, 5) == std::vector<std::int32_t>{0, 0, 0, 0, 0});
    CHECK(xoroshiroBounded(0, 16, 5) == std::vector<std::int32_t>{15, 2, 2, 4, 14});
    CHECK(xoroshiroBounded(0, 100, 5) == std::vector<std::int32_t>{96, 18, 15, 31, 88});
    CHECK(xoroshiroBounded(0, 7, 5) == std::vector<std::int32_t>{6, 1, 1, 2, 6});
    CHECK(xoroshiroBounded(42, 16, 5) == std::vector<std::int32_t>{6, 5, 13, 7, 10});
    CHECK(xoroshiroBounded(42, 100, 5) == std::vector<std::int32_t>{41, 31, 85, 48, 66});
    CHECK(xoroshiroBounded(42, 7, 5) == std::vector<std::int32_t>{2, 2, 6, 3, 4});
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

TEST_CASE("XoroshiroRandomSource full sequence matches vanilla for seed 42") {
    RandomSource random = RandomSource::xoroshiro(42);
    CHECK(random.nextFloat() == 0.7454321F);
    CHECK(random.nextFloat() == 0.39799505F);
    CHECK(random.nextFloat() == 0.59110755F);
    CHECK(random.nextDouble() == 0.2650272295535885);
    CHECK(random.nextDouble() == 0.45643019714958855);
    CHECK(random.nextDouble() == 0.6480715228911569);
    CHECK(random.nextBoolean());
    CHECK_FALSE(random.nextBoolean());
    CHECK(random.nextBoolean());
    CHECK(random.nextGaussian() == 3.1556335355420093);
    CHECK(random.nextGaussian() == -0.07157280316880266);
    CHECK(random.nextGaussian() == -0.017170294951799683);
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

TEST_CASE("Mth.getSeed matches vanilla") {
    CHECK(positionalSeed(0, 0, 0) == 0LL);
    CHECK(positionalSeed(1, 2, 3) == -33674130277896LL);
    CHECK(positionalSeed(-5, 64, 100) == -131967588646891LL);
    CHECK(positionalSeed(123, -45, 678) == -49831574764955LL);
    CHECK(positionalSeed(1000000, 0, -1000000) == 52603072207110LL);
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
