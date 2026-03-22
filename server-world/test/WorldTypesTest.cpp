#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/WorldTypes.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::world;

namespace {

struct Report {
    std::int32_t solid;
    std::int64_t checksum;
    std::int64_t heights;
};

[[nodiscard]] Report reportOf(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t solid = 0;
    const auto fold = [&checksum](std::int64_t value) {
        checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
    };
    for (std::int32_t y = chunk.verticalBounds().minY(); y <= chunk.verticalBounds().maxY(); ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t state = chunk.blockStateId(x, y, z);
                if (state != 0) {
                    ++solid;
                    fold(x);
                    fold(y);
                    fold(z);
                    fold(state);
                }
            }
        }
    }
    std::string heights;
    for (const std::int32_t height : chunk.heightmap()) {
        heights += std::to_string(height) + ",";
    }
    std::uint64_t heightChecksum = 14695981039346656037ULL;
    for (const char character : heights) {
        heightChecksum =
            (heightChecksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    return Report{solid, static_cast<std::int64_t>(checksum), static_cast<std::int64_t>(heightChecksum)};
}

void check(const Chunk& chunk, std::int32_t solid, std::int64_t checksum, std::int64_t heights) {
    const Report report = reportOf(chunk);
    CHECK(report.solid == solid);
    CHECK(report.checksum == checksum);
    CHECK(report.heights == heights);
}

} // namespace

TEST_CASE("the flat preset lays the stack the frozen kotlin lays") {
    SUBCASE("origin") { check(flatWorld::generate(0, 0), 1024, 3741048005682530213LL, -1473201475880939227LL); }
    SUBCASE("far") { check(flatWorld::generate(-13, 7), 1024, 3741048005682530213LL, -1473201475880939227LL); }
    SUBCASE("nether") {
        check(flatWorld::generate(0, 0, kNetherHeight), 1024, 2496904668097573925LL, 7258231024539104037LL);
    }
}

TEST_CASE("the void platform lands where the frozen kotlin puts it") {
    SUBCASE("origin") { check(voidWorld::generate(0, 0), 81, -5545503949188726749LL, -6109584414266244830LL); }
    // The platform straddles the chunk edge, so the far side of it lands in the next chunk over.
    SUBCASE("edge") { check(voidWorld::generate(-1, -1, 0, 64, 0), 16, 3258254307230174965LL, 5135350262354277701LL); }
    SUBCASE("neighbour") { check(voidWorld::generate(1, 0), 0, -3750763034362895579LL, -4614599026817265371LL); }
    SUBCASE("wide") {
        check(voidWorld::generate(0, 0, 8, 64, 8, 20), 256, -497872705087323739LL, -6017373917387464923LL);
    }
    SUBCASE("bare") {
        check(voidWorld::generate(0, 0, 8, 64, 8, 0), 0, -3750763034362895579LL, -4614599026817265371LL);
    }
    SUBCASE("deep") { check(voidWorld::generate(0, 0, 8, -60, 8), 81, -8975271358634552553LL, 8555298040030212233LL); }
}
