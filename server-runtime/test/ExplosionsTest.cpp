#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::server::world::ChunkBlockEntity;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() {
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                state.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
    }

    /// A solid cube of `path` from (from) to (to), inclusive.
    void fill(const std::string& path, std::int32_t from, std::int32_t to) {
        for (std::int32_t x = from; x <= to; ++x) {
            for (std::int32_t y = from; y <= to; ++y) {
                for (std::int32_t z = from; z <= to; ++z) {
                    REQUIRE(state.setBlockState(x, y, z, stateOf(path)));
                }
            }
        }
    }
};

} // namespace

TEST_CASE("an explosion in the open takes nothing out") {
    Loaded loaded;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 64.5, 8.5, 5.0F, random);

    CHECK(broken.empty());
}

TEST_CASE("an explosion in stone takes out a ball of it") {
    Loaded loaded;
    loaded.fill("stone", 0, 15);
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 5.0F, random);

    REQUIRE_FALSE(broken.empty());
    // Everything it named is gone and was stone.
    for (const explosions::Broken& block : broken) {
        CHECK(block.stateId == stateOf("stone"));
        CHECK(loaded.state.blockState(block.x, block.y, block.z) == registry::blocks::air().stateId);
    }
    // And the hole is roughly round, centred where it went off.
    double furthest = 0.0;
    for (const explosions::Broken& block : broken) {
        const double dx = block.x + 0.5 - 8.5;
        const double dy = block.y + 0.5 - 8.5;
        const double dz = block.z + 0.5 - 8.5;
        furthest = std::max(furthest, std::sqrt((dx * dx) + (dy * dy) + (dz * dz)));
    }
    // Stone costs (6 + 0.3) * 0.3 a block, so a charge of five only gets a couple of blocks in.
    CHECK(furthest >= 1.0);
    CHECK(furthest < 6.0);
}

TEST_CASE("a bigger charge takes out more") {
    const auto countAt = [](float power) {
        Loaded loaded;
        loaded.fill("stone", 0, 15);
        rng::RandomSource random = rng::RandomSource::legacy(1);
        return explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, power, random).size();
    };

    CHECK(countAt(2.0F) < countAt(5.0F));
    CHECK(countAt(5.0F) < countAt(8.0F));
}

TEST_CASE("what resists an explosion stops it, and what cannot be broken is never taken") {
    Loaded loaded;
    loaded.fill("obsidian", 0, 15);
    // A pocket of air around the charge so it has somewhere to start.
    REQUIRE(loaded.state.setBlockState(8, 8, 8, registry::blocks::air().stateId));
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 5.0F, random);

    // parity: obsidian's resistance drains a ray on the first block it meets.
    CHECK(broken.empty());

    Loaded bedrock;
    bedrock.fill("bedrock", 0, 15);
    REQUIRE(bedrock.state.setBlockState(8, 8, 8, registry::blocks::air().stateId));
    rng::RandomSource other = rng::RandomSource::legacy(1);
    CHECK(explosions::explode(bedrock.state, -64, 319, 8.5, 8.5, 8.5, 100.0F, other).empty());
}

TEST_CASE("a source that holds resistance down takes out what would have stopped it") {
    // parity: `WitherSkull.getBlockExplosionResistance`, which answers at most `0.8` for anything the
    // wither may break: that is what lets a blue skull through obsidian.
    Loaded loaded;
    loaded.fill("obsidian", 0, 15);
    REQUIRE(loaded.state.setBlockState(8, 8, 8, registry::blocks::air().stateId));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    const explosions::Resistance held = [](std::int32_t, float resistance) { return std::min(0.8F, resistance); };

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 1.0F, random, {}, held);

    CHECK_FALSE(broken.empty());
}

TEST_CASE("an explosion stops at the floor and the roof of the world") {
    Loaded loaded;
    for (std::int32_t x = 0; x <= 15; ++x) {
        for (std::int32_t z = 0; z <= 15; ++z) {
            REQUIRE(loaded.state.setBlockState(x, 8, z, stateOf("stone")));
        }
    }
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // A world one block tall: every ray leaves it immediately, and only that layer can go.
    const std::vector<explosions::Broken> broken = explosions::explode(loaded.state, 8, 8, 8.5, 8.5, 8.5, 5.0F, random);

    for (const explosions::Broken& block : broken) {
        CHECK(block.y == 8);
    }
}

TEST_CASE("the same charge in the same place takes out the same blocks") {
    const auto once = []() {
        Loaded loaded;
        loaded.fill("stone", 0, 15);
        rng::RandomSource random = rng::RandomSource::legacy(42);
        const std::vector<explosions::Broken> broken =
            explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 5.0F, random);
        std::vector<std::array<std::int32_t, 3>> positions;
        for (const explosions::Broken& block : broken) {
            positions.push_back({block.x, block.y, block.z});
        }
        return positions;
    };

    CHECK(once() == once());
    // And the order is its own, not whatever a hash happened to give.
    const std::vector<std::array<std::int32_t, 3>> positions = once();
    const std::set<std::array<std::int32_t, 3>> distinct{positions.begin(), positions.end()};
    CHECK(distinct.size() == positions.size());
}

TEST_CASE("the damage falls off with distance and stops at the radius") {
    // parity: `hurtEntities`' own arithmetic, at the centre and on the way out.
    CHECK(explosions::damageAt(0.0, 5.0F, 10.0F) == doctest::Approx(1.0 + (7.0 * 5.0)));
    CHECK(explosions::damageAt(10.0, 5.0F, 10.0F) == doctest::Approx(0.0F));
    CHECK(explosions::damageAt(11.0, 5.0F, 10.0F) == doctest::Approx(0.0F));

    const float near = explosions::damageAt(1.0, 5.0F, 10.0F);
    const float far = explosions::damageAt(9.0, 5.0F, 10.0F);
    CHECK(near > far);
    CHECK(far > 0.0F);

    // Half way out, the shape is the one the formula names.
    const double nearness = 0.5;
    CHECK(explosions::damageAt(5.0, 4.0F, 10.0F) ==
          doctest::Approx((((nearness * nearness) + nearness) / 2.0 * 7.0 * 4.0) + 1.0));
}

TEST_CASE("a bed goes off with the power of a bed") { CHECK(explosions::kBedPower == doctest::Approx(5.0F)); }

TEST_CASE("one ray for each cell on the shell of a sixteen-cubed grid, and no more") {
    Loaded loaded;
    rng::RandomSource used = rng::RandomSource::legacy(5);

    static_cast<void>(explosions::explode(loaded.state, -64, 319, 8.5, 64.5, 8.5, 5.0F, used));

    // parity: the shell of a 16-cubed grid, which is the cube less the 14-cubed inside of it. Each
    // ray draws exactly one float for its own power, so the count is visible in the stream.
    rng::RandomSource reference = rng::RandomSource::legacy(5);
    for (std::int32_t ray = 0; ray < (16 * 16 * 16) - (14 * 14 * 14); ++ray) {
        static_cast<void>(reference.nextFloat());
    }
    CHECK(used.nextFloat() == doctest::Approx(reference.nextFloat()));
    CHECK(used.nextLong() == reference.nextLong());
}

TEST_CASE("a hole blown at negative coordinates comes back at negative coordinates") {
    Loaded loaded;
    for (std::int32_t x = -16; x <= -1; ++x) {
        for (std::int32_t y = -16; y <= -1; ++y) {
            for (std::int32_t z = -16; z <= -1; ++z) {
                REQUIRE(loaded.state.setBlockState(x, y, z, stateOf("stone")));
            }
        }
    }
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, -8.5, -8.5, -8.5, 5.0F, random);

    REQUIRE_FALSE(broken.empty());
    for (const explosions::Broken& block : broken) {
        // The three coordinates survive the packing, sign and all, and name the block that went.
        CHECK(block.x < 0);
        CHECK(block.y < 0);
        CHECK(block.z < 0);
        CHECK(block.x >= -16);
        CHECK(block.y >= -16);
        CHECK(block.z >= -16);
        CHECK(loaded.state.blockState(block.x, block.y, block.z) == registry::blocks::air().stateId);
    }
    // The block it went off inside is one of them.
    bool centre = false;
    for (const explosions::Broken& block : broken) {
        centre = centre || (block.x == -9 && block.y == -9 && block.z == -9);
    }
    CHECK(centre);
}

TEST_CASE("the floor and the roof of the world are each their own bound") {
    Loaded loaded;
    for (std::int32_t x = 0; x <= 15; ++x) {
        for (std::int32_t y = 6; y <= 10; ++y) {
            for (std::int32_t z = 0; z <= 15; ++z) {
                REQUIRE(loaded.state.setBlockState(x, y, z, stateOf("stone")));
            }
        }
    }

    // A world whose floor is the layer the charge sits in: nothing under it can go.
    rng::RandomSource fromFloor = rng::RandomSource::legacy(1);
    for (const explosions::Broken& block : explosions::explode(loaded.state, 8, 319, 8.5, 8.5, 8.5, 6.0F, fromFloor)) {
        CHECK(block.y >= 8);
    }

    Loaded roofed;
    for (std::int32_t x = 0; x <= 15; ++x) {
        for (std::int32_t y = 6; y <= 10; ++y) {
            for (std::int32_t z = 0; z <= 15; ++z) {
                REQUIRE(roofed.state.setBlockState(x, y, z, stateOf("stone")));
            }
        }
    }
    rng::RandomSource fromRoof = rng::RandomSource::legacy(1);
    for (const explosions::Broken& block : explosions::explode(roofed.state, -64, 8, 8.5, 8.5, 8.5, 6.0F, fromRoof)) {
        CHECK(block.y <= 8);
    }
}

TEST_CASE("the power a ray starts with is the charge scaled by its own roll") {
    // parity: `radius * (0.7 + nextFloat() * 0.6)`, which is what makes one blast lopsided.
    Loaded weak;
    weak.fill("stone", 0, 15);
    rng::RandomSource low = rng::RandomSource::legacy(1);
    const std::size_t small = explosions::explode(weak.state, -64, 319, 8.5, 8.5, 8.5, 4.0F, low).size();

    Loaded strong;
    strong.fill("stone", 0, 15);
    rng::RandomSource high = rng::RandomSource::legacy(1);
    const std::size_t large = explosions::explode(strong.state, -64, 319, 8.5, 8.5, 8.5, 4.5F, high).size();

    CHECK(small <= large);
    CHECK(small > 0);
}

TEST_CASE("the rays go out in every direction, not only one") {
    Loaded loaded;
    loaded.fill("stone", 0, 15);
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 6.0F, random);

    // parity: the directions run from -1 to 1 on each axis, so the hole opens both ways.
    bool westOfIt = false;
    bool eastOfIt = false;
    bool belowIt = false;
    bool aboveIt = false;
    bool northOfIt = false;
    bool southOfIt = false;
    for (const explosions::Broken& block : broken) {
        westOfIt = westOfIt || block.x < 8;
        eastOfIt = eastOfIt || block.x > 8;
        belowIt = belowIt || block.y < 8;
        aboveIt = aboveIt || block.y > 8;
        northOfIt = northOfIt || block.z < 8;
        southOfIt = southOfIt || block.z > 8;
    }
    CHECK(westOfIt);
    CHECK(eastOfIt);
    CHECK(belowIt);
    CHECK(aboveIt);
    CHECK(northOfIt);
    CHECK(southOfIt);
}

TEST_CASE("a primed cart spares the rails it ran on and what holds them up") {
    // parity: `MinecartTNT.shouldBlockExplode` leaves the rail standing, and its resistance twin
    // charges the ray nothing for crossing it.
    Loaded track;
    track.fill("stone", 0, 15);
    for (std::int32_t z = 0; z <= 15; ++z) {
        REQUIRE(track.state.setBlockState(8, 9, z, stateOf("rail")));
    }
    rng::RandomSource random = rng::RandomSource::legacy(3);
    static_cast<void>(explosions::explode(track.state, -64, 319, 8.5, 9.5, 8.5, 5.0F, random, explosions::sparesRails));
    for (std::int32_t z = 0; z <= 15; ++z) {
        CHECK(track.state.blockState(8, 9, z) == stateOf("rail"));
        CHECK(track.state.blockState(8, 8, z) == stateOf("stone"));
    }

    // With nobody sparing them, the same blast takes the rail out.
    Loaded loose;
    loose.fill("stone", 0, 15);
    for (std::int32_t z = 0; z <= 15; ++z) {
        REQUIRE(loose.state.setBlockState(8, 9, z, stateOf("rail")));
    }
    rng::RandomSource other = rng::RandomSource::legacy(3);
    static_cast<void>(explosions::explode(loose.state, -64, 319, 8.5, 9.5, 8.5, 5.0F, other));
    CHECK(loose.state.blockState(8, 9, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a blast that takes out a playing jukebox reports the song stopped") {
    Loaded loaded;
    loaded.fill("stone", 0, 15);
    REQUIRE(loaded.state.setBlockState(8, 8, 8, stateOf("jukebox")));
    // The block a playing jukebox shows is its has_record one, which is the state the blast reports.
    const std::optional<BlockChange> started =
        jukeboxes::insert(loaded.state, 8, 8, 8, basalt::wire::common::Identifier::vanilla("music_disc_13"));
    REQUIRE(started.has_value());
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 5.0F, random);

    const auto jukebox = std::ranges::find_if(
        broken, [&started](const explosions::Broken& block) { return block.stateId == started->stateId; });
    REQUIRE(jukebox != broken.end());
    CHECK(jukebox->songWasPlaying);
}

TEST_CASE("a jukebox whose song already ended says nothing") {
    Loaded loaded;
    loaded.fill("stone", 0, 15);
    REQUIRE(loaded.state.setBlockState(8, 8, 8, stateOf("jukebox")));
    const std::optional<BlockChange> started =
        jukeboxes::insert(loaded.state, 8, 8, 8, basalt::wire::common::Identifier::vanilla("music_disc_13"));
    REQUIRE(started.has_value());
    // A finished song leaves the disc in but takes the counter out, which is what says it is playing.
    Chunk* chunk = loaded.state.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    ChunkBlockEntity* entity = chunk->blockEntityAt(8, 8, 8);
    REQUIRE(entity != nullptr);
    static_cast<void>(entity->data().remove("ticks_since_song_started"));
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<explosions::Broken> broken =
        explosions::explode(loaded.state, -64, 319, 8.5, 8.5, 8.5, 5.0F, random);

    const auto jukebox = std::ranges::find_if(
        broken, [&started](const explosions::Broken& block) { return block.stateId == started->stateId; });
    REQUIRE(jukebox != broken.end());
    CHECK_FALSE(jukebox->songWasPlaying);
}
