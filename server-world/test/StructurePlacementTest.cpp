#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/structure/ConcentricRingsPlacement.hpp>
#include <basalt/server/world/structure/StructurePlacementGrid.hpp>

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world::structure;

namespace {

constexpr std::int64_t kSeed = 8675309LL;

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

struct Run {
    std::int32_t placement = 0;
    std::int32_t structure = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(const StructurePlacementGrid& grid, std::int64_t seed) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkX = -48; chunkX <= 48; ++chunkX) {
        for (std::int32_t chunkZ = -48; chunkZ <= 48; ++chunkZ) {
            const auto [candidateX, candidateZ] = grid.candidateChunk(seed, chunkX, chunkZ);
            checksum =
                (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(candidateX))) * 1099511628211ULL;
            checksum =
                (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(candidateZ))) * 1099511628211ULL;
            if (grid.isPlacementChunk(seed, chunkX, chunkZ)) {
                ++result.placement;
            }
            if (grid.isStructureChunk(seed, chunkX, chunkZ)) {
                ++result.structure;
                // Which chunks won, not only how many: two salts can agree on the count.
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkX))) * 1099511628211ULL;
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkZ))) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the structure grids place what the frozen kotlin places") {
    SUBCASE("village") {
        const Run result = run(StructurePlacementGrid{34, 8, 10387312}, kSeed);
        CHECK(result.placement == 9);
        CHECK(result.structure == 9);
        CHECK(result.checksum == -7072875333996138025LL);
    }

    SUBCASE("triangular") {
        const Run result = run(StructurePlacementGrid{32, 8, 10387313, SpreadType::Triangular}, kSeed);
        CHECK(result.placement == 9);
        CHECK(result.structure == 9);
        CHECK(result.checksum == 6997176872496123332LL);
    }

    SUBCASE("freq_DEFAULT") {
        const Run result = run(
            StructurePlacementGrid{24, 6, 165745296, SpreadType::Linear, 0.25F, FrequencyReduction::Default}, kSeed);
        CHECK(result.placement == 16);
        CHECK(result.structure == 3);
        CHECK(result.checksum == 6056199791861200377LL);
    }

    SUBCASE("freq_LEGACY_TYPE_1") {
        const Run result =
            run(StructurePlacementGrid{24, 6, 165745296, SpreadType::Linear, 0.25F, FrequencyReduction::LegacyType1},
                kSeed);
        CHECK(result.placement == 16);
        CHECK(result.structure == 3);
        CHECK(result.checksum == 6720007298525256331LL);
    }

    SUBCASE("freq_LEGACY_TYPE_2") {
        const Run result =
            run(StructurePlacementGrid{24, 6, 165745296, SpreadType::Linear, 0.25F, FrequencyReduction::LegacyType2},
                kSeed);
        CHECK(result.placement == 16);
        CHECK(result.structure == 6);
        CHECK(result.checksum == -7603550954305024221LL);
    }

    SUBCASE("freq_LEGACY_TYPE_3") {
        const Run result =
            run(StructurePlacementGrid{24, 6, 165745296, SpreadType::Linear, 0.25F, FrequencyReduction::LegacyType3},
                kSeed);
        CHECK(result.placement == 16);
        CHECK(result.structure == 6);
        CHECK(result.checksum == 7477512345056084539LL);
    }

    SUBCASE("excluded") {
        const StructurePlacementGrid villages{34, 8, 10387312};
        const StructurePlacementGrid grid{20,
                                          5,
                                          165745296,
                                          SpreadType::Linear,
                                          1.0F,
                                          FrequencyReduction::Default,
                                          std::make_shared<const ExclusionZone>(
                                              6, [&villages]() -> const StructurePlacementGrid& { return villages; })};
        const Run result = run(grid, kSeed);
        CHECK(result.placement == 23);
        CHECK(result.structure == 20);
        CHECK(result.checksum == 7655968969395496927LL);
    }
}

TEST_CASE("the stronghold rings land where the frozen kotlin lands them") {
    // The biome pattern is deliberately coarse: what matters is that some cells match and some do
    // not, so the reservoir pick is exercised rather than always taking the first.
    const ConcentricRingsPlacement placement{
        32, 3, 128, {"plains"}, [](std::int32_t quartX, std::int32_t, std::int32_t quartZ) -> std::string_view {
            return floorMod(quartX * 7 + quartZ * 3, 5) == 0 ? "plains" : "ocean";
        }};
    const std::vector<std::pair<std::int32_t, std::int32_t>> positions = placement.ringPositions(4242LL);
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const auto& [x, z] : positions) {
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(x))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(z))) * 1099511628211ULL;
    }
    std::string first;
    for (std::size_t index = 0; index < 4 && index < positions.size(); ++index) {
        if (!first.empty()) {
            first += ";";
        }
        first += std::to_string(positions[index].first) + "," + std::to_string(positions[index].second);
    }
    CHECK(positions.size() == 128);
    CHECK(first == "74,63;-98,40;31,-162;233,-208");
    CHECK(static_cast<std::int64_t>(checksum) == -3218392041393038267LL);

    const ConcentricRingsPlacement never{
        32, 3, 8, {"plains"}, [](std::int32_t, std::int32_t, std::int32_t) -> std::string_view { return "ocean"; }};
    const std::vector<std::pair<std::int32_t, std::int32_t>> fallback = never.ringPositions(4242LL);
    std::string fallbackFirst;
    for (std::size_t index = 0; index < 4 && index < fallback.size(); ++index) {
        if (!fallbackFirst.empty()) {
            fallbackFirst += ";";
        }
        fallbackFirst += std::to_string(fallback[index].first) + "," + std::to_string(fallback[index].second);
    }
    CHECK(fallback.size() == 8);
    CHECK(fallbackFirst == "69,58;-92,33;28,-157;232,-205");

    const ConcentricRingsPlacement empty{
        32, 3, 0, {}, [](std::int32_t, std::int32_t, std::int32_t) -> std::string_view { return "ocean"; }};
    CHECK(empty.ringPositions(1LL).empty());
}
