#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

/// The parameters vanilla loads from datapack JSON, replaced by a synthetic set derived from the
/// name. The Kotlin oracle derives them the same way, so what this pins is the shape of the graph
/// and not Mojang's numbers.
std::shared_ptr<const noise::NormalNoise> syntheticNoise(std::string_view id) {
    // FNV-1a over the name: a stable seed both implementations reproduce.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : id) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    basalt::server::random::RandomSource source =
        basalt::server::random::RandomSource::xoroshiro(static_cast<std::int64_t>(hash));
    const std::array<double, 3> amplitudes = {1.0, 0.6, 0.3};
    return std::make_shared<const noise::NormalNoise>(
        noise::NormalNoise::create(source, -(static_cast<std::int32_t>(id.size()) % 5 + 1), amplitudes));
}

Function base3d() { return fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0); }

[[nodiscard]] std::int64_t bits(double value) { return std::bit_cast<std::int64_t>(value); }

struct Expectation {
    std::string_view label;
    const Function& function;
    std::int64_t minBits;
    std::int64_t maxBits;
    std::array<std::int64_t, 6> samples;
};

} // namespace

TEST_CASE("the three noise routers match the frozen kotlin bit for bit") {
    const NoiseLookup lookup = syntheticNoise;
    const OverworldNoiseRouter overworld = noiseRouterData::overworld(lookup, base3d());
    const NetherNoiseRouter nether = noiseRouterData::nether(lookup, base3d());
    const EndNoiseRouter end = noiseRouterData::end(base3d(), 0x5DEECE66DLL);

    const std::vector<Expectation> expectations = {
        {"ow.continents",
         overworld.continents,
         -4607343261643852070,
         4616028775210923738,
         {-4627662361178379715, -4629088872540359446, 4603161249303035660, -4636315111126101848, -4628948730316283784,
          -4622629150917447616}},
        {"ow.erosion",
         overworld.erosion,
         -4607343261643852070,
         4616028775210923738,
         {-4629517344952467747, 4593729784388303736, 4576409413882267470, 4604306892147851850, 4594255638330860835,
          4598449615615869978}},
        {"ow.ridges",
         overworld.ridges,
         -4607343261643852070,
         4616028775210923738,
         {4597086953515202925, -4623571839842067172, 4597390002632501695, 4599832938853731551, 4603685501359408408,
          -4642696496146795642}},
        {"ow.depth",
         overworld.depth,
         -4604709258854924288,
         4612803474025873408,
         {4605786302703468544, 4603143646991613952, 4594536547981721600, -4624182217017393152, -4619038690960736256,
          -4615174900657684480}},
        {"ow.temperature",
         overworld.temperature,
         -4607343261643852070,
         4616028775210923738,
         {-4625277848001057062, 4593632095380040148, 4595509739922024770, 4588407964466120144, -4642377085155461572,
          4595880376849566029}},
        {"ow.vegetation",
         overworld.vegetation,
         -4607343261643852070,
         4616028775210923738,
         {4595496681310145464, 4598585812942252277, 4600067113899607125, -4637705634585420521, -4641429340179529182,
          4596397096630086946}},
        {"ow.veinToggle",
         overworld.veinToggle,
         -4607343261643852070,
         4616028775210923738,
         {4601571589196218921, -4622177004340924213, -4634047870314211060, 0, 0, 0}},
        {"ow.veinRidged",
         overworld.veinRidged,
         -4632943008797425664,
         4615848631229855450,
         {4604012686192068689, 4598625155960286842, 4600058318031282826, -4632943008797425664, -4632943008797425664,
          -4632943008797425664}},
        {"ow.veinGap",
         overworld.veinGap,
         -4607343261643852070,
         4616028775210923738,
         {-4621677560913201323, -4625154889854944544, 4604063400016919882, 4601710521205036100, 4600466423307680572,
          -4632027576148214890}},
        {"ow.barrier",
         overworld.barrierNoise,
         -4607343261643852070,
         4616028775210923738,
         {4602898497650994710, 4598573766395162809, -4633665221161266959, -4628040916473985924, 4600620408686208199,
          -4634771897358141826}},
        {"ow.final",
         overworld.finalDensity,
         -4621443817620023979,
         4601928219234751829,
         {4586401110930630118, 4589738932102281144, 4584967735025010240, -4621443817620023979, -4621443817620023979,
          -4621443817620023979}},
        {"ow.surface",
         overworld.preliminarySurfaceLevel,
         -4589168020290535424,
         4644337115725824000,
         {4629700416936869888, 4633078116657397760, 4633078116657397760, 4631952216750555136, 4633078116657397760,
          4629700416936869888}},
        {"nether.temperature",
         nether.temperature,
         -4607343261643852070,
         4616028775210923738,
         {-4622362832592611128, 4597939173269726149, 4598250163999645812, -4624774721070836449, -4632565024287778408,
          -4622885076505707032}},
        {"nether.vegetation",
         nether.vegetation,
         -4607343261643852070,
         4616028775210923738,
         {4601267080397327252, -4622383655823263383, -4633074342907632855, -4632546630376245730, 4588428114774505827,
          4591840384435331530}},
        {"nether.final",
         nether.finalDensity,
         -4621443817620023979,
         4601928219234751829,
         {4601928219234751829, 4601928219234751829, -4638114834622645615, 4575099563671264183, 4598913809884165177,
          4598913809884165177}},
        {"end.erosion",
         end.erosion,
         -4617596992938311680,
         4603241769126068224,
         {-4617596992938311680, -4617596992938311680, -4619728769890910208, -4617596992938311680, -4617596992938311680,
          -4617596992938311680}},
        {"end.final",
         end.finalDensity,
         -4621443817620023979,
         4601928219234751829,
         {-4633313429737927869, -4633313429737927869, -4626075728471370248, -4621443817620023979, -4621443817620023979,
          -4621443817620023979}},
    };

    for (const Expectation& expectation : expectations) {
        CAPTURE(expectation.label);
        CHECK(bits(expectation.function->minValue()) == expectation.minBits);
        CHECK(bits(expectation.function->maxValue()) == expectation.maxBits);
        for (std::size_t i = 0; i < expectation.samples.size(); ++i) {
            CAPTURE(i);
            densityfunction::FunctionContext context{static_cast<std::int32_t>(i) * 137 - 400,
                                                     static_cast<std::int32_t>(i) * 53 - 60,
                                                     static_cast<std::int32_t>(i) * -211 + 300, nullptr};
            CHECK(bits(expectation.function->compute(context)) == expectation.samples[i]);
        }
    }
}
