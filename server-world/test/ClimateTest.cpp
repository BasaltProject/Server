#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/OverworldBiomes.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <thread>

using namespace basalt::server::world::climate;

TEST_CASE("a parameter quantises by truncation, the way vanilla does") {
    CHECK(quantizeCoord(0.5F) == 5000);
    CHECK(quantizeCoord(-0.5F) == -5000);
    CHECK(quantizeCoord(0.00009F) == 0);
    CHECK(quantizeCoord(-0.00009F) == 0);
    CHECK(unquantizeCoord(5000) == doctest::Approx(0.5F));
}

TEST_CASE("the tree agrees with a full scan of the same points") {
    std::vector<std::pair<ParameterPoint, int>> values;
    for (int i = 0; i < 60; ++i) {
        const auto step = static_cast<float>(i);
        values.emplace_back(parameters(step * 0.03F - 0.9F, step * 0.017F - 0.5F, step * -0.021F + 0.6F,
                                       step * 0.011F - 0.3F, step * 0.007F, step * -0.013F + 0.4F, 0.0F),
                            i);
    }
    const auto tree = RTree<int>::create(values);

    for (int probe = 0; probe < 40; ++probe) {
        const auto step = static_cast<float>(probe);
        const TargetPoint point = target(step * 0.045F - 0.85F, step * 0.02F - 0.4F, step * -0.03F + 0.5F,
                                         step * 0.015F - 0.25F, step * 0.005F, step * -0.02F + 0.3F);
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        int scanned = -1;
        for (const auto& [candidate, value] : values) {
            const std::int64_t fitness = candidate.fitness(point);
            if (fitness < best) {
                best = fitness;
                scanned = value;
            }
        }
        CAPTURE(probe);
        CHECK(tree->search(point.toArray()) == scanned);
    }
}

TEST_CASE("two threads searching one tree do not seed each other") {
    std::vector<std::pair<ParameterPoint, int>> values;
    for (int i = 0; i < 30; ++i) {
        const auto step = static_cast<float>(i);
        values.emplace_back(parameters(step * 0.06F - 0.9F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), i);
    }
    const auto tree = RTree<int>::create(values);

    const auto probe = [&tree](float temperature) {
        return tree->search(target(temperature, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F).toArray());
    };
    const int fromMain = probe(-0.9F);

    int fromOther = -1;
    std::thread worker{[&] { fromOther = probe(-0.9F); }};
    worker.join();
    CHECK(fromMain == fromOther);
    CHECK(probe(-0.9F) == fromMain);
}

TEST_CASE("the overworld list matches the frozen kotlin entry for entry") {
    // Order matters as much as values: ties go to the point registered first.
    const std::vector<std::pair<ParameterPoint, BiomeName>> values = overworldBiomes::build();
    CHECK(values.size() == 7594);

    std::string digest;
    digest.reserve(values.size() * 64);
    const auto append = [&digest](std::int64_t value) {
        digest += std::to_string(value);
        digest += ',';
    };
    for (const auto& [point, biome] : values) {
        append(point.temperature.min);
        append(point.temperature.max);
        append(point.humidity.min);
        append(point.humidity.max);
        append(point.continentalness.min);
        append(point.continentalness.max);
        append(point.erosion.min);
        append(point.erosion.max);
        append(point.depth.min);
        append(point.depth.max);
        append(point.weirdness.min);
        append(point.weirdness.max);
        append(point.offset);
        digest += biome;
        digest += ';';
    }

    auto hash = static_cast<std::uint64_t>(-0x340d631b7bdddcdbLL);
    for (const char ch : digest) {
        hash ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(ch));
        hash *= 0x100000001B3ULL;
    }
    CHECK(static_cast<std::int64_t>(hash) == -3349918987853375340LL);

    CHECK(digest.substr(0, digest.find(';')) ==
          "-10000,10000,-10000,10000,-12000,-10500,-10000,10000,0,0,-10000,10000,0,mushroom_fields");
    CHECK(values.back().second == "deep_dark");
}
