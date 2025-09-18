#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <doctest.h>

#include <cstdint>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

[[nodiscard]] SimpleBlockFeature::StateProvider constant(std::string_view path) {
    return [path](basalt::server::random::RandomSource&, const DecorationPos&) { return stateIdOf(path); };
}

[[nodiscard]] Chunk groundChunk() {
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 63; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // A patchwork of supports so every branch of canSurvive is reached.
            std::int32_t ground = 0;
            switch ((x + z) % 6) {
            case 0:
                ground = stateIdOf("grass_block");
                break;
            case 1:
                ground = stateIdOf("sand");
                break;
            case 2:
                ground = stateIdOf("podzol");
                break;
            case 3:
                ground = registry::blocks::stone().stateId;
                break;
            case 4:
                ground = registry::blocks::water().stateId;
                break;
            default:
                ground = stateIdOf("mycelium");
                break;
            }
            chunk.setBlockStateId(x, 63, z, ground);
        }
    }
    return chunk;
}

[[nodiscard]] std::int64_t fold(const Chunk& chunk, std::int32_t minY, std::int32_t maxY) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = minY; y <= maxY; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunk.blockStateId(x, y, z)))) *
                    1099511628211ULL;
            }
        }
    }
    return static_cast<std::int64_t>(checksum);
}

[[nodiscard]] std::tuple<std::int32_t, std::int64_t, std::int32_t> run(const SimpleBlockFeature& feature) {
    Chunk chunk = groundChunk();
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(555LL));
    std::int32_t placed = 0;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            if (feature.place(chunk, random, DecorationPos{x, 64, z})) {
                ++placed;
            }
        }
    }
    return {placed, fold(chunk, 62, 66), random.nextInt()};
}

} // namespace

TEST_CASE("the single-block vegetation feature places what the frozen kotlin places") {
    SUBCASE("poppy") {
        const auto [placed, checksum, after] =
            run(SimpleBlockFeature{[](basalt::server::random::RandomSource& r, const DecorationPos&) {
                return r.nextInt(3) < 2 ? stateIdOf("poppy") : stateIdOf("dandelion");
            }});
        CHECK(placed == 127);
        CHECK(checksum == 4942062415265984999LL);
        CHECK(after == 1789394068);
    }

    SUBCASE("deadBush") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("dead_bush")});
        CHECK(placed == 169);
        CHECK(checksum == 5656803359154873394LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("lilyPad") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("lily_pad")});
        CHECK(placed == 43);
        CHECK(checksum == -2648131822595495688LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("mushroom") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("red_mushroom")});
        CHECK(placed == 213);
        CHECK(checksum == 501162659521026725LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("leafLitter") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("leaf_litter")});
        CHECK(placed == 213);
        CHECK(checksum == -4046527946432313433LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("tallGrass") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("tall_grass"), true});
        CHECK(placed == 127);
        CHECK(checksum == 103875343881789033LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("paleMoss") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("pale_moss_carpet")});
        CHECK(placed == 256);
        CHECK(checksum == 4446049513061390016LL);
        CHECK(after == -1367481220);
    }

    SUBCASE("tickedGrass") {
        const auto [placed, checksum, after] = run(SimpleBlockFeature{constant("short_grass"), false, true});
        CHECK(placed == 127);
        CHECK(checksum == 1459387768364207288LL);
        CHECK(after == -1367481220);
    }
}

TEST_CASE("the pale moss carpet grows sides and a topper against a wall") {
    // The carpet only grows sides, and only then draws for its topper, where there is a wall to
    // attach to. Without one every side short-circuits to "none" and the topper path never runs.
    Chunk chunk = groundChunk();
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            if (x % 4 == 0 || z % 5 == 0) {
                for (std::int32_t y = 64; y <= 67; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
                }
            }
        }
    }
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(555LL));
    const SimpleBlockFeature carpet{constant("pale_moss_carpet")};
    std::int32_t placed = 0;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            if (carpet.place(chunk, random, DecorationPos{x, 64, z})) {
                ++placed;
            }
        }
    }
    CHECK(placed == 144);
    CHECK(fold(chunk, 62, 68) == 4554191091728307524LL);
    CHECK(random.nextInt() == 435936527);
}
