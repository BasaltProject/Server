#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MonumentDoubleRooms.hpp>
#include <basalt/server/world/structure/MonumentRoomFitters.hpp>
#include <basalt/server/world/structure/MonumentShell.hpp>
#include <basalt/server/world/structure/MonumentSingleRooms.hpp>
#include <basalt/server/world/structure/MonumentWings.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

constexpr StructureBoundingBox kRoomBox{0, 20, 0, 15, 40, 15};

/// Deep water on a single course of stone: the pillars then run all the way to the world's floor and
/// meet the guard that stops them, and the packed ice gives the water box something it is not
/// allowed to touch.
[[nodiscard]] Chunk seaChunk() {
    const std::int32_t packedIce = registry::blocks::named("packed_ice")->stateId;
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, -64, z, registry::blocks::stone().stateId);
            for (std::int32_t y = -63; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
            for (std::int32_t y = 55; y <= 56; ++y) {
                chunk.setBlockStateId(x, y, z, packedIce);
            }
            // A shelf of stone above sea level, so how far the shell floods is observable.
            for (std::int32_t y = 60; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -64; y <= 70; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::water().stateId &&
                    id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.generatedEntities().size()};
}

/// A lattice room to draw against: the graph's own rooms, so the doorways are the real ones. The
/// graph owns them, so it is kept alive here for as long as the test runs.
[[nodiscard]] const MonumentRoom* roomOf(MonumentRoomKind kind) {
    static std::vector<MonumentRoomGraph> held;
    for (std::int64_t seed = 1; seed <= 64; ++seed) {
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed));
        MonumentRoomGraph graph = generateMonumentRoomGraph(random);
        const std::vector<MonumentRoomPiece> fitted = fitMonumentRooms(graph, random);
        for (const MonumentRoomPiece& piece : fitted) {
            if (piece.kind == kind && piece.room != nullptr) {
                held.push_back(std::move(graph));
                return piece.room;
            }
        }
    }
    throw std::runtime_error{"no such room in the first 64 seeds"};
}

struct Run {
    std::int32_t written = 0;
    std::size_t entities = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(StructureBoundingBox origin,
                      const std::function<void(const MonumentBrush&, rng::RandomSource&)>& draw) {
    Chunk chunk = seaChunk();
    // A sea level below the vanilla floor of 64, so the shell's own `max` is observable.
    const StructureBoundingBox clip{0, -64, 0, 15, 70, 15};
    const StructurePieceFill fill{origin, StructureDirection::North, chunk, clip};
    const MonumentBrush brush{fill, 40, clip};
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(4711));
    draw(brush, random);
    Run result;
    const auto [checksum, written, entities] = fold(chunk);
    result.written = written;
    result.entities = entities;
    result.deferred = chunk.takeGeneratedBlockWrites().size();
    result.checksum = checksum;
    result.after = random.nextInt();
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t entities, std::size_t deferred, std::int64_t checksum,
           std::int32_t after) {
    CHECK(result.written == written);
    CHECK(result.entities == entities);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
    CHECK(result.after == after);
}

} // namespace

TEST_CASE("every monument piece draws what the frozen kotlin draws") {
    SUBCASE("shell") {
        const Run result = run(StructureBoundingBox{-20, 25, -47, 37, 48, 10},
                               [](const MonumentBrush& brush, rng::RandomSource& random) {
                                   static_cast<void>(random);
                                   monumentShell::place(brush, 40);
                               });
        check(result, 1424, 0, 0, 8046015927167620368LL, 185365340);
    }

    SUBCASE("entry") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::entry(brush, *roomOf(MonumentRoomKind::Entry));
        });
        check(result, 620, 0, 0, 4027648566380795693LL, 185365340);
    }

    SUBCASE("core") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::core(brush);
        });
        check(result, 1154, 0, 0, -2391992880893800953LL, 185365340);
    }

    SUBCASE("simple0") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::simple(brush, *roomOf(MonumentRoomKind::Simple), 0, random);
        });
        check(result, 640, 0, 0, -4815082219343645563LL, 185365340);
    }

    SUBCASE("simple1") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::simple(brush, *roomOf(MonumentRoomKind::Simple), 1, random);
        });
        check(result, 596, 0, 0, -2650606400975282775LL, 1091621030);
    }

    SUBCASE("simple2") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::simple(brush, *roomOf(MonumentRoomKind::Simple), 2, random);
        });
        check(result, 592, 0, 0, 1230362543047553841LL, 1091621030);
    }

    SUBCASE("simpleTop") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentSingleRooms::simpleTop(brush, *roomOf(MonumentRoomKind::SimpleTop), random);
        });
        check(result, 722, 0, 0, 8265808800738315259LL, -437567476);
    }

    SUBCASE("doubleX") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentDoubleRooms::doubleX(brush, *roomOf(MonumentRoomKind::DoubleX));
        });
        check(result, 858, 0, 0, 5087381730340241459LL, 185365340);
    }

    SUBCASE("doubleXy") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentDoubleRooms::doubleXy(brush, *roomOf(MonumentRoomKind::DoubleXY));
        });
        check(result, 990, 0, 0, 2578441631325336187LL, 185365340);
    }

    SUBCASE("doubleY") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentDoubleRooms::doubleY(brush, *roomOf(MonumentRoomKind::DoubleY));
        });
        check(result, 810, 0, 0, 8642414345260455729LL, 185365340);
    }

    SUBCASE("doubleYz") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentDoubleRooms::doubleYz(brush, *roomOf(MonumentRoomKind::DoubleYZ));
        });
        check(result, 906, 0, 0, -3370281530483490899LL, 185365340);
    }

    SUBCASE("doubleZ") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentDoubleRooms::doubleZ(brush, *roomOf(MonumentRoomKind::DoubleZ));
        });
        check(result, 688, 0, 0, -8433678566451819823LL, 185365340);
    }

    SUBCASE("wingHall") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentWings::wing(brush, 0);
        });
        check(result, 690, 0, 0, -5605110002191888600LL, 185365340);
    }

    SUBCASE("wingGallery") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentWings::wing(brush, 1);
        });
        check(result, 620, 1, 0, -2736078192357195363LL, 185365340);
    }

    SUBCASE("penthouse") {
        const Run result = run(kRoomBox, [](const MonumentBrush& brush, rng::RandomSource& random) {
            static_cast<void>(random);
            monumentWings::penthouse(brush);
        });
        check(result, 828, 1, 0, 8504074545653886635LL, 185365340);
    }
}
