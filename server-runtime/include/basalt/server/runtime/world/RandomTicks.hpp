#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// The blocks vanilla picks out of the air each tick and lets act.
///
/// parity: `ServerLevel.tickChunk`'s `tickBlocks` half. Which positions come up is not a matter of
/// taste: the same `randValue * 3 + 1013904223` walk decides it, and how many sections it steps over
/// is part of that walk.
namespace randomTicks {

/// parity: `Level.getBlockRandomPos`'s own multiplier and addend.
inline constexpr std::int32_t kWalkMultiplier = 3;
inline constexpr std::int32_t kWalkAddend = 1013904223;

/// Asked before a random tick changes a block, and answers whether it may.
///
/// One place rather than forty: the families that live in this file all end in one block becoming
/// another, so the question is asked where that happens and `cause` says which kind it was:
/// `fade` for something being taken away, `grow` for anything else. The families that live in their
/// own files write their own blocks and are not asked.
using MayChange = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t from, std::int32_t to,
                                     std::string_view cause)>;

/// Everything a sweep needs besides the blocks themselves.
///
/// A struct rather than a world object: what these families read off a world is its name, its seed,
/// where it ends, whether it rains on it and what its rules say, and taking those makes the whole
/// thing testable without one.
struct Context {
    WorldState& world;
    /// Which walk this world gets. Two worlds picking positions from one walk is not vanilla.
    std::string_view worldName;
    /// Where the walk starts, the first time this world is ticked.
    std::int64_t seed = 0;
    /// Where the world ends. Only the pitcher crop asks, and only to leave itself room.
    std::int32_t maxY = 319;
    const GameRuleValues& gameRules;
    const WorldClock& clock;
    /// The world's own word for it: `peaceful`, `easy`, `normal`, anything else is hard.
    std::string_view difficulty = "normal";
    bool hasSkylight = true;
    /// parity: `FarmlandBlock.randomTick`, which counts rain as water however far the sea is.
    bool raining = false;
    /// parity: the `water_evaporates` dimension attribute, which is the Nether's: ice melting there
    /// leaves nothing behind rather than water.
    bool evaporates = false;
    /// parity: the `spawns_portal_piglins` dimension attribute, which is the overworld's.
    bool spawnsPortalPiglins = false;

    /// How a block that spawns something gets an id for it. Unset, nothing is spawned.
    std::function<std::int32_t()> allocateEntityId;
    /// Unset, every change goes through, which is what a server with no plugins does.
    MayChange mayChange;
    /// `#minecraft:flowers`, which a growing sapling looks for. Unset, there are none.
    saplingGrowth::Flowering flowering;
};

/// parity: `CropBlock.getGrowthSpeed`, farmland underneath and the neighbours' rows and all.
///
/// Public because a stem grows at the speed of the crop it is, and `StemGrowth` lives elsewhere.
[[nodiscard]] float growthSpeed(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                const wire::common::Identifier& block);

} // namespace randomTicks

/// One walk, kept between ticks, and the dispatch that says what each block does with its turn.
///
/// An object rather than free functions because of that walk: vanilla keeps one per level and which
/// positions come up depends on where it has got to.
class RandomTicks {
public:
    /// Runs one tick's worth over `chunks`, returning what changed.
    ///
    /// parity: `randomTickSpeed` positions per section of each chunk near a player, and the section
    /// is skipped altogether when nothing in it ticks.
    [[nodiscard]] std::vector<BlockChange> tick(const randomTicks::Context& context,
                                                std::span<const std::array<std::int32_t, 2>> chunks, std::int32_t speed,
                                                random::RandomSource& random);

    /// One position's worth, for a caller that already knows where: what `tick` does once it has
    /// picked. A state that ticks at random and has no case here answers with nothing.
    [[nodiscard]] std::vector<BlockChange> tickBlock(const randomTicks::Context& context, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, random::RandomSource& random);

private:
    /// parity: `Level.randValue`, one walk per level, kept between ticks.
    ///
    /// Vanilla seeds it from a fresh random and this seeds it from the world's own, so the same
    /// world picks the same positions every run: deterministic where vanilla is merely arbitrary.
    std::map<std::string, std::int32_t, std::less<>> walks_;

    [[nodiscard]] std::int32_t nextWalk(const randomTicks::Context& context);
};

} // namespace basalt::server::runtime::world
