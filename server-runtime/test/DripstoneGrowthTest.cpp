#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DripstoneGrowth.hpp>
#include <basalt/server/runtime/world/StalactiteDrips.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t spikeOf(const std::string& direction, const std::string& thickness,
                                   const std::string& waterlogged = "false") {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("pointed_dripstone"),
        {{"vertical_direction", direction}, {"thickness", thickness}, {"waterlogged", waterlogged}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// parity: `PointedDripstoneBlock.canGrow` asks for a water source over the dripstone block, which every
/// case here stands under.
/// Runs the tick until something happens, since the chance is about one in a hundred.
void tickUntilChanged(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, rng::RandomSource& random,
                      std::vector<BlockChange>& changes, std::int32_t attempts = 20000) {
    for (std::int32_t at = 0; at < attempts && changes.empty(); ++at) {
        dripstoneGrowth::dripstone(world, x, y, z, *world.blockState(x, y, z), random, changes);
    }
}

} // namespace

TEST_CASE("a stalactite grows a tip downwards or drips a stalagmite up") {
    // The two halves are a coin toss, so both are reached from the same sequence.
    bool sawTip = false;
    bool sawStalagmite = false;
    for (std::int32_t seed = 0; seed < 12 && !(sawTip && sawStalagmite); ++seed) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
        REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
        REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
        REQUIRE(world.setBlockState(4, 60, 4, stateOf("stone")));

        rng::RandomSource random = rng::RandomSource::legacy(seed);
        std::vector<BlockChange> changes;
        tickUntilChanged(world, 4, 69, 4, random, changes);
        REQUIRE(changes.size() == 1);

        if (pathOf(*world.blockState(4, 68, 4)) == "pointed_dripstone") {
            sawTip = true;
            CHECK(propertyOf(*world.blockState(4, 68, 4), "vertical_direction") == "down");
            CHECK(propertyOf(*world.blockState(4, 68, 4), "thickness") == "tip");
        } else {
            sawStalagmite = true;
            // parity: it grows from the floor up, ten blocks down at most.
            CHECK(pathOf(*world.blockState(4, 61, 4)) == "pointed_dripstone");
            CHECK(propertyOf(*world.blockState(4, 61, 4), "vertical_direction") == "up");
        }
    }
    CHECK(sawTip);
    CHECK(sawStalagmite);
}

TEST_CASE("only the block hanging from the dripstone grows") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "frustum")));
    REQUIRE(world.setBlockState(4, 68, 4, spikeOf("down", "tip")));

    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    // parity: `isStalactiteStartPos`, so the tick on the tip itself does nothing: the whole spike
    // is grown from its topmost block.
    for (std::int32_t at = 0; at < 20000; ++at) {
        dripstoneGrowth::dripstone(world, 4, 68, 4, *world.blockState(4, 68, 4), random, changes);
    }
    CHECK(changes.empty());

    // From the top it grows, and what it grows is one more block under the tip.
    tickUntilChanged(world, 4, 69, 4, random, changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].y == 67);
}

TEST_CASE("what stops a stalactite growing") {
    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;

    // Pointing up: a stalagmite does not grow itself.
    {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
        REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
        REQUIRE(world.setBlockState(4, 69, 4, spikeOf("up", "tip")));
        for (std::int32_t at = 0; at < 20000; ++at) {
            dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
        }
        CHECK(changes.empty());
    }

    // Hanging from stone rather than from dripstone.
    {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(4, 70, 4, stateOf("stone")));
        REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
        for (std::int32_t at = 0; at < 20000; ++at) {
            dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
        }
        CHECK(changes.empty());
    }

    // A waterlogged tip is not free hanging.
    {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
        REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
        REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip", "true")));
        for (std::int32_t at = 0; at < 20000; ++at) {
            dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
        }
        CHECK(changes.empty());
    }

    // Something already under the tip.
    {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
        REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
        REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
        REQUIRE(world.setBlockState(4, 68, 4, stateOf("stone")));
        for (std::int32_t at = 0; at < 20000; ++at) {
            dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
        }
        CHECK(changes.empty());
    }
}

/// Ticks the stalactite many times, undoing every downward tip so the drip half of the coin toss
/// is reached over and over: what is under test is where the drip lands, not which half comes up.
void dripOnly(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, rng::RandomSource& random) {
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 40000; ++at) {
        dripstoneGrowth::dripstone(world, x, y, z, *world.blockState(x, y, z), random, changes);
        if (!changes.empty() && changes[0].y == y - 1) {
            REQUIRE(world.setBlockState(x, y - 1, z, registry::blocks::air().stateId));
        }
        changes.clear();
    }
}

TEST_CASE("a floor too far down grows nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 90, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 91, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 89, 4, spikeOf("down", "tip")));
    // parity: ten blocks below the tip is as far as a drip looks, so this floor is one too far.
    REQUIRE(world.setBlockState(4, 78, 4, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(5);
    dripOnly(world, 4, 89, 4, random);
    CHECK(pathOf(*world.blockState(4, 79, 4)) != "pointed_dripstone");

    // One block closer and it does reach: the ten is a reach and not a rounding.
    WorldState near = emptyWorld();
    near.getOrGenerateChunk(0, 0);
    REQUIRE(near.setBlockState(4, 90, 4, stateOf("dripstone_block")));
    REQUIRE(near.setBlockState(4, 91, 4, stateOf("water")));
    REQUIRE(near.setBlockState(4, 89, 4, spikeOf("down", "tip")));
    REQUIRE(near.setBlockState(4, 79, 4, stateOf("stone")));
    dripOnly(near, 4, 89, 4, random);
    CHECK(pathOf(*near.blockState(4, 80, 4)) == "pointed_dripstone");
}

TEST_CASE("a drip needs somewhere solid to land on") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
    // A torch is neither air nor water, and its top is not something a stalagmite stands on.
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("torch")));
    REQUIRE(world.setBlockState(4, 60, 4, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(9);
    dripOnly(world, 4, 69, 4, random);
    CHECK(pathOf(*world.blockState(4, 65, 4)) != "pointed_dripstone");
    // And it stops there rather than carrying on to the stone below.
    CHECK(pathOf(*world.blockState(4, 61, 4)) != "pointed_dripstone");
}

TEST_CASE("a sulfur spike reaches two blocks and no further") {
    // parity: each spike carries its own `getMaxGrowthLength`, and sulfur's is two.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("sulfur")));
    const auto sulfurSpike = [](const std::string& thickness) {
        const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
            Identifier::vanilla("sulfur_spike"),
            {{"vertical_direction", "down"}, {"thickness", thickness}, {"waterlogged", "false"}}, std::nullopt);
        REQUIRE(state.has_value());
        return *state;
    };
    // Four blocks of spike: the tip is past the reach of the block that would grow it.
    REQUIRE(world.setBlockState(4, 69, 4, sulfurSpike("frustum")));
    REQUIRE(world.setBlockState(4, 68, 4, sulfurSpike("middle")));
    REQUIRE(world.setBlockState(4, 67, 4, sulfurSpike("frustum")));
    REQUIRE(world.setBlockState(4, 66, 4, sulfurSpike("tip")));

    rng::RandomSource random = rng::RandomSource::legacy(4);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 40000; ++at) {
        dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("water under a stalactite stops the drip") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
    REQUIRE(world.setBlockState(4, 66, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 60, 4, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(2);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 20000; ++at) {
        dripstoneGrowth::dripstone(world, 4, 69, 4, *world.blockState(4, 69, 4), random, changes);
        // Only the downward tip may happen; the drip cannot reach past the water.
        if (!changes.empty() && changes[0].y == 68) {
            changes.clear();
            REQUIRE(world.setBlockState(4, 68, 4, registry::blocks::air().stateId));
        }
    }
    CHECK(pathOf(*world.blockState(4, 61, 4)) != "pointed_dripstone");
}

TEST_CASE("a stalactite under water books the cauldron under it, which then fills a level at a time") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 71, 4, stateOf("water")));
    REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
    REQUIRE(world.setBlockState(4, 65, 4, stateOf("cauldron")));

    // parity: `maybeTransferFluid` with the float under a water drip's chance, and the cauldron's
    // turn fifty ticks plus the fall away.
    const std::optional<stalactiteDrips::Transfer> drip =
        stalactiteDrips::transfer(world, 4, 69, 4, *world.blockState(4, 69, 4), 0.1F, false);
    REQUIRE(drip.has_value());
    CHECK(drip->tip == stalactiteDrips::Position{4, 69, 4});
    REQUIRE(drip->cauldron.has_value());
    CHECK(*drip->cauldron == stalactiteDrips::Position{4, 65, 4});
    CHECK(drip->delay == 54);
    CHECK_FALSE(stalactiteDrips::transfer(world, 4, 69, 4, *world.blockState(4, 69, 4), 0.2F, false).has_value());

    const std::optional<stalactiteDrips::Fill> first = stalactiteDrips::fillCauldron(world, 4, 65, 4, false);
    REQUIRE(first.has_value());
    CHECK(pathOf(first->stateId) == "water_cauldron");
    CHECK(propertyOf(first->stateId, "level") == "1");
    CHECK(first->levelEvent == stalactiteDrips::kWaterIntoCauldron);
    REQUIRE(world.setBlockState(4, 65, 4, first->stateId));
    const std::optional<stalactiteDrips::Fill> second = stalactiteDrips::fillCauldron(world, 4, 65, 4, false);
    REQUIRE(second.has_value());
    CHECK(propertyOf(second->stateId, "level") == "2");
}

TEST_CASE("lava drips less often and only into an empty cauldron, and mud over the root dries to clay") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(4, 71, 4, stateOf("lava")));
    REQUIRE(world.setBlockState(4, 69, 4, spikeOf("down", "tip")));
    REQUIRE(world.setBlockState(4, 65, 4, stateOf("cauldron")));
    const std::int32_t tip = *world.blockState(4, 69, 4);
    CHECK(stalactiteDrips::transfer(world, 4, 69, 4, tip, 0.05F, false).has_value());
    CHECK_FALSE(stalactiteDrips::transfer(world, 4, 69, 4, tip, 0.06F, false).has_value());
    const std::optional<stalactiteDrips::Fill> lava = stalactiteDrips::fillCauldron(world, 4, 65, 4, false);
    REQUIRE(lava.has_value());
    CHECK(pathOf(lava->stateId) == "lava_cauldron");

    REQUIRE(world.setBlockState(4, 71, 4, stateOf("mud")));
    const std::optional<stalactiteDrips::Transfer> dried = stalactiteDrips::transfer(world, 4, 69, 4, tip, 0.1F, false);
    REQUIRE(dried.has_value());
    CHECK(dried->clay == stalactiteDrips::Position{4, 71, 4});
    // parity: where water evaporates the mud holds none.
    CHECK_FALSE(stalactiteDrips::transfer(world, 4, 69, 4, tip, 0.1F, true).has_value());
}
