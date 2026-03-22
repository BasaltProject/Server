#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/climate/OverworldBiomes.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <thread>

using namespace basalt::server::world::climate;

TEST_CASE("a parameter quantises by truncation, the way vanilla does") {
    // Truncation toward zero, not to nearest: the difference is a biome boundary one block over.
    CHECK(quantizeCoord(0.5F) == 5000);
    CHECK(quantizeCoord(-0.5F) == -5000);
    CHECK(quantizeCoord(0.00009F) == 0);
    CHECK(quantizeCoord(-0.00009F) == 0);
    CHECK(unquantizeCoord(5000) == doctest::Approx(0.5F));
}

TEST_CASE("distance is zero inside a span and grows outside it") {
    const Parameter span = parameterSpan(-0.5F, 0.5F);
    CHECK(span.distance(0) == 0);
    CHECK(span.distance(quantizeCoord(0.5F)) == 0);
    CHECK(span.distance(quantizeCoord(0.75F)) == quantizeCoord(0.25F));
    CHECK(span.distance(quantizeCoord(-0.75F)) == quantizeCoord(0.25F));
}

TEST_CASE("a span covers both of the parameters it was made from") {
    const Parameter joined = parameterSpan(-0.5F, 0.0F).span(parameterSpan(0.25F, 0.75F));
    CHECK(joined.min == quantizeCoord(-0.5F));
    CHECK(joined.max == quantizeCoord(0.75F));
}

TEST_CASE("a point that is asked for exactly is the one that comes back") {
    std::vector<std::pair<ParameterPoint, std::string>> values = {
        {parameters(-1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "cold"},
        {parameters(1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "hot"},
        {parameters(0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "wet"},
        {parameters(0.0F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F), "dry"},
        {parameters(0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F), "inland"},
        {parameters(0.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F, 0.0F), "ocean"},
        {parameters(0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F), "eroded"},
    };
    const auto tree = RTree<std::string>::create(values);
    CHECK(tree->search(target(-1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F).toArray()) == "cold");
    CHECK(tree->search(target(1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F).toArray()) == "hot");
    CHECK(tree->search(target(0.0F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F).toArray()) == "ocean");
    // Something between two points takes the nearer one.
    CHECK(tree->search(target(0.9F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F).toArray()) == "hot");
}

TEST_CASE("the tree agrees with a full scan of the same points") {
    // The descent exists to avoid the scan; if the two ever disagree, the descent is the bug.
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
    // The seed is per thread on purpose: sharing it would let one worker's last answer decide
    // another's tie, and a tie between two equally distant regions is a real biome boundary.
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
    // And the main thread still answers the same after the other has run.
    CHECK(probe(-0.9F) == fromMain);
}

TEST_CASE("a tree with nothing in it is refused") {
    CHECK_THROWS_AS(static_cast<void>(RTree<int>::create({})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(parameterSpan(1.0F, -1.0F)), std::invalid_argument);
}

TEST_CASE("the overworld list matches the frozen kotlin entry for entry") {
    // 7594 points whose *order* matters as much as their values: ties go to whichever was
    // registered first. A checksum over the whole rendering, so one changed field or one swapped
    // pair moves it. The reference comes from the Kotlin oracle.
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

    // Named explicitly too, so a failure says which end moved rather than only that one did.
    CHECK(digest.substr(0, digest.find(';')) ==
          "-10000,10000,-10000,10000,-12000,-10500,-10000,10000,0,0,-10000,10000,0,mushroom_fields");
    CHECK(values.back().second == "deep_dark");
}

TEST_CASE("the built list picks a plausible biome for a few known climates") {
    const auto values = overworldBiomes::build();
    std::vector<std::pair<ParameterPoint, BiomeName>> owned{values.begin(), values.end()};
    const auto tree = RTree<BiomeName>::create(std::move(owned));

    // Deep, cold, far out to sea.
    CHECK(tree->search(target(-0.9F, 0.0F, -1.0F, 0.0F, 0.0F, 0.0F).toArray()) == "deep_frozen_ocean");
    // Hot and dry, well inland. At weirdness zero this is the valleys slice, so it is a river and
    // not a desert however hot it is: the desert wants to be off the valley floor.
    CHECK(tree->search(target(0.9F, -0.9F, 0.6F, 0.2F, 0.0F, 0.0F).toArray()) == "river");
    CHECK(tree->search(target(0.9F, -0.9F, 0.6F, 0.2F, 0.0F, 0.5F).toArray()) == "desert");
    // Off the edge of the continentalness axis is the mushroom island, as vanilla has it.
    CHECK(tree->search(target(0.0F, 0.0F, -1.15F, 0.0F, 0.0F, 0.0F).toArray()) == "mushroom_fields");
}
