#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <stdexcept>
#include <string>

using namespace basalt::server::world::densityfunction;

namespace {

class NoNoise final : public CompileContext {
public:
    std::shared_ptr<const basalt::server::world::noise::Noise> createNoise(const std::string&) override {
        throw std::logic_error{"no noise"};
    }

    basalt::server::random::RandomSource createRandom(std::string_view) override {
        throw std::logic_error{"no random"};
    }

    basalt::server::random::RandomSource createEndIslandRandom() override { throw std::logic_error{"no random"}; }
};

float sampleAtY(const std::string& json, std::int32_t y) {
    DensityFunctionLoader loader{nullptr};
    const auto parsed = basalt::wire::common::parseJson(json);
    REQUIRE(parsed.has_value());
    const auto function = loader.parse(*parsed);
    REQUIRE(function.has_value());
    DensityCompiler compiler{std::make_unique<NoNoise>()};
    SamplerContext context{SamplerContext::Options{}};
    return compiler.sampler(*function)->sampleValue(context, 0, y, 0);
}

std::string gradientY() {
    return R"({"type":"minecraft:gradient","axis":"y","from_coordinate":0,"to_coordinate":100,)"
           R"("from_value":0,"to_value":100})";
}

} // namespace

TEST_CASE("round nodes follow Math.round, floor, ceil and truncate") {
    const std::string input = R"("input":{"type":"minecraft:add","left":)" + gradientY() + R"(,"right":-2.5})";
    const auto at = [&](const char* type, std::int32_t y) {
        return sampleAtY(std::string{R"({"type":"minecraft:)"} + type + R"(",)" + input + "}", y);
    };
    CHECK(at("round", 0) == -2.0F);
    CHECK(at("floor", 0) == -3.0F);
    CHECK(at("ceil", 0) == -2.0F);
    CHECK(at("truncate", 0) == -2.0F);
    CHECK(at("round", 5) == 3.0F);
    CHECK(at("truncate", 5) == 2.0F);
}

TEST_CASE("round with a multiple snaps to it, pow follows Math.pow") {
    const std::string snapped = R"({"type":"minecraft:round","input":)" + gradientY() + R"(,"multiple":4})";
    CHECK(sampleAtY(snapped, 3) == 4.0F);
    CHECK(sampleAtY(snapped, 5) == 4.0F);
    const std::string power = R"({"type":"minecraft:pow","base":)" + gradientY() + R"(,"exponent":2.5})";
    CHECK(sampleAtY(power, 4) == 32.0F);
}
