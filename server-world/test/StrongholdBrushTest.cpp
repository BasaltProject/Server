#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StrongholdBrush.hpp>

#include <doctest.h>

#include <cstdint>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] Chunk stoneChunk(bool hollow) {
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 40; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // A cave through the middle, so the shell's skip-air rule and its skipped draw show.
            if (hollow) {
                for (std::int32_t y = 12; y <= 16; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 0; y <= 40; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(StrongholdDoor door, bool hollow, bool skipAir) {
    Chunk chunk = stoneChunk(hollow);
    const StructureBoundingBox box{1, 8, 1, 13, 20, 13};
    const StructureBoundingBox clip{0, 0, 0, 15, 40, 15};
    const StructurePieceFill fill{box, StructureDirection::North, chunk, clip};
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(20250831LL));
    const StrongholdBrush brush{fill, chunk, random};
    brush.shell(0, 0, 0, 8, 8, 8, skipAir);
    brush.door(door, 2, 1, 0);
    brush.box(3, 2, 3, 5, 4, 5, strongholdPalette::cobblestone());
    brush.maybe(0.5F, 4, 5, 4, strongholdPalette::smoothStoneSlab());
    brush.maybe(0.5F, 4, 6, 4, strongholdPalette::stoneBrickSlab());
    brush.createChest(4, 1, 4, "minecraft:chests/stronghold_corridor");
    // Against a wall and then into a corner: one solid neighbour turns the chest, two leave it
    // facing north.
    brush.createChest(1, 1, 4, "minecraft:chests/stronghold_corridor");
    brush.createChest(1, 1, 1, "minecraft:chests/stronghold_corridor");
    brush.silverfishSpawner(6, 1, 6);
    Run result;
    const auto [checksum, written, blockEntities] = fold(chunk);
    result.written = written;
    result.blockEntities = blockEntities;
    result.deferred = chunk.takeGeneratedBlockWrites().size();
    result.checksum = checksum;
    result.after = random.nextInt();
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t deferred,
           std::int64_t checksum, std::int32_t after) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
    CHECK(result.after == after);
}

} // namespace

TEST_CASE("the stronghold brush draws what the frozen kotlin draws") {
    SUBCASE("opening") {
        check(run(StrongholdDoor::Opening, false, true), 729, 4, 0, 374317734422466963LL, 1556979993);
    }

    SUBCASE("woodDoor") {
        check(run(StrongholdDoor::WoodDoor, false, true), 729, 4, 0, -2130769668714880324LL, 1556979993);
    }

    SUBCASE("grates") {
        check(run(StrongholdDoor::Grates, false, true), 729, 4, 0, -3739463078296862090LL, 1556979993);
    }

    SUBCASE("ironDoor") {
        check(run(StrongholdDoor::IronDoor, false, true), 730, 4, 0, -2759895567937183918LL, 1556979993);
    }

    SUBCASE("hollow") {
        check(run(StrongholdDoor::Opening, true, true), 334, 4, 0, -3128848843180299488LL, -484170814);
    }

    SUBCASE("hollowFilled") {
        check(run(StrongholdDoor::Opening, true, false), 729, 4, 0, 7268136418783640560LL, 1556979993);
    }
}
