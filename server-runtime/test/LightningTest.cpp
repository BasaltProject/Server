#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

using Position = std::array<std::int32_t, 3>;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t rod(const std::string& powered) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("lightning_rod"), {{"powered", powered}, {"facing", "up"}, {"waterlogged", "false"}},
        stateOf("lightning_rod"));
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string propertyAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& name) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() {
        for (std::int32_t chunkX = -1; chunkX <= 10; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 10; ++chunkZ) {
                state.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
    }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
    }
};

} // namespace

TEST_CASE("a strike with no rod around it lands on the top of the column") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    // parity: the motion-blocking heightmap, which is one above the block itself.
    CHECK(*target == Position{8, 65, 8});
    // A column outside the loaded world is not a target at all.
    CHECK_FALSE(lightning::targetFor(loaded.state, 500, 500).has_value());
}

TEST_CASE("a rod nearby takes the strike instead of the ground") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(12, 64, 8, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    CHECK(*target == Position{12, 65, 8});
}

TEST_CASE("the nearest rod is the one that takes it") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(12, 64, 8, rod("false"));
    loaded.put(10, 64, 8, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    CHECK(*target == Position{10, 65, 8});
}

TEST_CASE("a rod that is not the top of its own column is not a target") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(12, 64, 8, rod("false"));
    // Something over the rod, so its tip is no longer the top of the column.
    loaded.put(12, 66, 8, stateOf("stone"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    CHECK(*target == Position{8, 65, 8});
}

TEST_CASE("a strike powers the rod it hit and books its release") {
    Loaded loaded;
    loaded.put(8, 64, 8, rod("false"));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;

    lightning::strike(loaded.state, 8, 65, 8, false, random, changes);

    // parity: the block struck and the one under it are both offered the strike.
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].x == 8);
    CHECK(changes[0].y == 64);
    CHECK(changes[0].z == 8);
    CHECK(propertyAt(loaded.state, 8, 64, 8, "powered") == "true");
    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, lightning::rod()));
}

TEST_CASE("a strike straight onto a rod powers that one") {
    Loaded loaded;
    loaded.put(8, 64, 8, rod("false"));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;

    lightning::strike(loaded.state, 8, 64, 8, false, random, changes);

    CHECK(changes.size() == 1);
    CHECK(propertyAt(loaded.state, 8, 64, 8, "powered") == "true");
}

TEST_CASE("a strike lights a fire only where it is allowed to") {
    Loaded quiet;
    quiet.put(8, 63, 8, stateOf("stone"));
    rng::RandomSource peaceful = rng::RandomSource::legacy(1);
    std::vector<BlockChange> none;
    lightning::strike(quiet.state, 8, 64, 8, false, peaceful, none);
    CHECK(none.empty());
    CHECK(quiet.state.blockState(8, 64, 8) == registry::blocks::air().stateId);

    Loaded burning;
    burning.put(8, 63, 8, stateOf("stone"));
    rng::RandomSource harder = rng::RandomSource::legacy(1);
    std::vector<BlockChange> lit;
    lightning::strike(burning.state, 8, 64, 8, true, harder, lit);
    REQUIRE(lit.size() == 1);
    CHECK(registry::blockStateDefinitions::blockOf(lit[0].stateId)->path() == "fire");
}

TEST_CASE("a rod lets its signal go when its turn comes") {
    Loaded loaded;
    loaded.put(8, 64, 8, rod("true"));

    const std::optional<BlockChange> released = lightning::releaseRod(loaded.state, 8, 64, 8);

    REQUIRE(released.has_value());
    CHECK(released->x == 8);
    CHECK(released->y == 64);
    CHECK(released->z == 8);
    CHECK(propertyAt(loaded.state, 8, 64, 8, "powered") == "false");
}

TEST_CASE("a rod that is already quiet, and anything that is not a rod, release nothing") {
    Loaded loaded;
    loaded.put(8, 64, 8, rod("false"));
    loaded.put(9, 64, 8, stateOf("stone"));

    CHECK_FALSE(lightning::releaseRod(loaded.state, 8, 64, 8).has_value());
    CHECK_FALSE(lightning::releaseRod(loaded.state, 9, 64, 8).has_value());
    CHECK_FALSE(lightning::releaseRod(loaded.state, 500, 64, 500).has_value());
}

TEST_CASE("a chunk strikes about once in a hundred thousand turns") {
    rng::RandomSource random = rng::RandomSource::legacy(12345);
    std::int32_t struck = 0;
    for (std::int32_t turn = 0; turn < 400000; ++turn) {
        if (lightning::shouldStrike(random)) {
            ++struck;
        }
    }

    // parity: one in `kStrikeOdds`, which over four hundred thousand turns is a handful.
    CHECK(struck > 0);
    CHECK(struck < 20);
    CHECK(lightning::kStrikeOdds == 100000);
    CHECK(lightning::kRodTicks == 8);
    CHECK(lightning::kRodRange == 128);
}

TEST_CASE("a rod in another chunk is found at the place it actually stands") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    // Chunk one on both axes, at a local corner rather than in the middle of it.
    loaded.put(31, 64, 31, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    CHECK(*target == Position{31, 65, 31});

    // And the other corner of the same chunk, which the local loops reach from the other end.
    Loaded corner;
    corner.put(8, 64, 8, stateOf("stone"));
    corner.put(16, 64, 16, rod("false"));
    const std::optional<Position> other = lightning::targetFor(corner.state, 8, 8);
    REQUIRE(other.has_value());
    CHECK(*other == Position{16, 65, 16});
}

TEST_CASE("the search reaches a hundred and twenty-eight blocks and no further") {
    const auto takesRodAt = [](std::int32_t rodX) {
        Loaded loaded;
        loaded.put(8, 64, 8, stateOf("stone"));
        loaded.put(rodX, 64, 8, rod("false"));
        const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);
        REQUIRE(target.has_value());
        return (*target)[0] == rodX;
    };

    // parity: the distance is squared and compared with 128 squared, so 128 away is still in reach.
    CHECK(takesRodAt(8 + 128));
    CHECK_FALSE(takesRodAt(8 + 129));
    CHECK(takesRodAt(8 + 100));
}

TEST_CASE("two rods the same distance away leave the first one found holding it") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(4, 64, 8, rod("false"));
    loaded.put(12, 64, 8, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    // parity: a later rod has to be strictly nearer to take it, which makes the answer stable.
    REQUIRE(target.has_value());
    CHECK(*target == Position{4, 65, 8});
}

TEST_CASE("the column a strike is aimed at is the one it reads the height of") {
    Loaded loaded;
    // Two neighbouring columns of different heights, in a chunk that is not the origin.
    loaded.put(20, 64, 20, stateOf("stone"));
    loaded.put(21, 70, 20, stateOf("stone"));

    const std::optional<Position> low = lightning::targetFor(loaded.state, 20, 20);
    REQUIRE(low.has_value());
    CHECK(*low == Position{20, 65, 20});

    const std::optional<Position> high = lightning::targetFor(loaded.state, 21, 20);
    REQUIRE(high.has_value());
    CHECK(*high == Position{21, 71, 20});
}

TEST_CASE("the strike roll is the same one vanilla makes") {
    rng::RandomSource actual = rng::RandomSource::legacy(999);
    rng::RandomSource expected = rng::RandomSource::legacy(999);

    for (std::int32_t turn = 0; turn < 5000; ++turn) {
        // parity: a strike is the roll coming out zero, not any other single value.
        CHECK(lightning::shouldStrike(actual) == (expected.nextInt(100000) == 0));
    }
}

TEST_CASE("a strike far from the origin searches the chunks around itself") {
    Loaded loaded;
    loaded.put(136, 64, 136, stateOf("stone"));
    loaded.put(140, 64, 136, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 136, 136);

    REQUIRE(target.has_value());
    CHECK(*target == Position{140, 65, 136});
}

TEST_CASE("the search reaches as far along z as it does along x") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(8, 64, 8 + 128, rod("false"));

    const std::optional<Position> target = lightning::targetFor(loaded.state, 8, 8);

    REQUIRE(target.has_value());
    CHECK(*target == Position{8, 65, 136});
}

TEST_CASE("neighbouring columns along z are told apart as well") {
    Loaded loaded;
    loaded.put(20, 64, 20, stateOf("stone"));
    loaded.put(20, 70, 21, stateOf("stone"));

    const std::optional<Position> low = lightning::targetFor(loaded.state, 20, 20);
    REQUIRE(low.has_value());
    CHECK(*low == Position{20, 65, 20});

    const std::optional<Position> high = lightning::targetFor(loaded.state, 20, 21);
    REQUIRE(high.has_value());
    CHECK(*high == Position{20, 71, 21});
}

TEST_CASE("a strike is the roll landing on zero, and on nothing else") {
    std::optional<std::int64_t> strikes;
    std::optional<std::int64_t> misses;
    for (std::int64_t seed = 0; seed < 2000000 && !(strikes && misses); ++seed) {
        rng::RandomSource probe = rng::RandomSource::legacy(seed);
        const std::int32_t roll = probe.nextInt(100000);
        if (roll == 0 && !strikes) {
            strikes = seed;
        }
        if (roll == 1 && !misses) {
            misses = seed;
        }
    }
    REQUIRE(strikes.has_value());
    REQUIRE(misses.has_value());

    rng::RandomSource lucky = rng::RandomSource::legacy(*strikes);
    CHECK(lightning::shouldStrike(lucky));
    rng::RandomSource nearly = rng::RandomSource::legacy(*misses);
    CHECK_FALSE(lightning::shouldStrike(nearly));
}
