#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/world/Fires.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::runtime::world::fires {
namespace {

constexpr std::int32_t kOldEnoughToDie = 3;
constexpr float kRainBase = 0.2F;
constexpr float kRainPerAge = 0.03F;
constexpr std::int32_t kSideChance = 300;
constexpr std::int32_t kVerticalChance = 250;
constexpr std::int32_t kBaseRate = 100;
constexpr std::int32_t kIgniteBonus = 40;
constexpr std::int32_t kDifficultyWeight = 7;
constexpr std::int32_t kAgeWeight = 30;
constexpr std::int32_t kBurnAgeBias = 10;
constexpr std::int32_t kBurnAgeLimit = 5;

[[nodiscard]] std::int32_t stateAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return world.blockState(x, y, z).value_or(registry::blocks::air().stateId);
}

[[nodiscard]] std::int32_t ageOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("age");
    if (found == properties.end()) {
        return 0;
    }
    std::int32_t value = 0;
    const std::from_chars_result result =
        std::from_chars(found->second.data(), found->second.data() + found->second.size(), value);
    return result.ec == std::errc{} && result.ptr == found->second.data() + found->second.size() ? value : 0;
}

void put(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
         std::vector<BlockChange>& changes) {
    if (!world.setBlockState(x, y, z, stateId)) {
        return;
    }
    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = stateId});
}

/// Whether rain is falling on this block.
///
/// parity: `Level.isRainingAt` without its biome half: it asks whether the sky is open, which is the
/// part that decides whether a roof keeps a fire alight.
[[nodiscard]] bool rainingOn(const WorldState& world, const WorldWeather& weather, std::int32_t x, std::int32_t y,
                             std::int32_t z) {
    if (!weather.raining) {
        return false;
    }
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return false;
    }
    return chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlocking) <= y;
}

[[nodiscard]] std::int32_t difficultyOf(std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0;
    }
    if (difficulty == "easy") {
        return 1;
    }
    return difficulty == "normal" ? 2 : 3;
}

/// parity: `FireBlock.getStateForPlacement`. Fire standing on something that burns, or on a solid
/// face, is the plain default state and shows no sides at all; only fire with neither under it
/// reaches for its neighbours. There is no `down` property, which is the null the loop there skips.
[[nodiscard]] std::map<std::string, std::string> fireSides(const WorldState& world, std::int32_t x, std::int32_t y,
                                                           std::int32_t z) {
    const std::int32_t below = stateAt(world, x, y - 1, z);
    if (registry::fireOdds::igniteOdds(below) > 0 ||
        registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up)) {
        return {};
    }
    const auto burns = [](std::int32_t neighbour) {
        return registry::fireOdds::igniteOdds(neighbour) > 0 ? "true" : "false";
    };
    return {{"up", burns(stateAt(world, x, y + 1, z))},
            {"north", burns(stateAt(world, x, y, z - 1))},
            {"south", burns(stateAt(world, x, y, z + 1))},
            {"west", burns(stateAt(world, x - 1, y, z))},
            {"east", burns(stateAt(world, x + 1, y, z))}};
}

/// Soul fire where the ground calls for it, ordinary fire everywhere else.
///
/// parity: `BaseFireBlock.getState`, and the state id comes from the block's **default**, not from
/// the first of its palette: the five sides of a fire are `false` by default and `true` first.
[[nodiscard]] std::optional<std::int32_t> fireFor(const WorldState& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::int32_t age) {
    if (registry::blockConnections::isSoulFireBase(stateAt(world, x, y - 1, z))) {
        const registry::BlockState* soul = registry::blocks::named("soul_fire");
        return soul == nullptr ? std::nullopt : std::optional{soul->stateId};
    }
    std::map<std::string, std::string> properties = fireSides(world, x, y, z);
    properties.emplace("age", std::to_string(age));
    return registry::blocks::stateIdOf(wire::common::Identifier::vanilla("fire"), properties);
}

/// parity: `FireBlock.getIgniteOdds(level, pos)`: the best of the six around an empty block.
[[nodiscard]] std::int32_t igniteOddsAround(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (stateAt(world, x, y, z) != registry::blocks::air().stateId) {
        return 0;
    }
    std::int32_t odds = 0;
    for (const std::int32_t neighbour :
         {stateAt(world, x, y - 1, z), stateAt(world, x, y + 1, z), stateAt(world, x, y, z - 1),
          stateAt(world, x, y, z + 1), stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z)}) {
        odds = std::max(odds, registry::fireOdds::igniteOdds(neighbour));
    }
    return odds;
}

/// parity: `FireBlock.isValidFireLocation`, which is any of the six being flammable at all.
[[nodiscard]] bool hasSomethingToBurn(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    for (const std::int32_t neighbour :
         {stateAt(world, x, y - 1, z), stateAt(world, x, y + 1, z), stateAt(world, x, y, z - 1),
          stateAt(world, x, y, z + 1), stateAt(world, x - 1, y, z), stateAt(world, x + 1, y, z)}) {
        if (registry::fireOdds::burnOdds(neighbour) > 0) {
            return true;
        }
    }
    return false;
}

/// parity: the dimension's `infiniburn` tag, which in the Nether is what keeps fire alight.
[[nodiscard]] bool burnsForever(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const wire::common::Identifier* below = registry::blockStateDefinitions::blockOf(stateAt(world, x, y - 1, z));
    if (below == nullptr) {
        return false;
    }
    const std::string_view path = below->path();
    return path == "netherrack" || path == "magma_block" || path == "bedrock";
}

/// parity: `FireBlock.checkBurnOut`, which either takes the block or leaves fire in its place.
void burnOut(WorldState& world, const WorldWeather& weather, std::int32_t x, std::int32_t y, std::int32_t z,
             std::int32_t chance, random::RandomSource& random, std::int32_t age, std::vector<BlockChange>& changes) {
    const std::int32_t stateId = stateAt(world, x, y, z);
    if (random.nextInt(chance) >= registry::fireOdds::burnOdds(stateId)) {
        return;
    }
    if (random.nextInt(age + kBurnAgeBias) < kBurnAgeLimit && !rainingOn(world, weather, x, y, z)) {
        const std::optional<std::int32_t> fire =
            fireFor(world, x, y, z, std::min(age + random.nextInt(5) / 4, kMaxAge));
        if (!fire.has_value()) {
            return;
        }
        put(world, x, y, z, *fire, changes);
        schedule(world, x, y, z, random);
        return;
    }
    put(world, x, y, z, registry::blocks::air().stateId, changes);
}

/// parity: the 3x3 column from one below to four above, with the odds thinning as it climbs.
void spread(WorldState& world, const WorldWeather& weather, std::string_view difficulty, std::int32_t x, std::int32_t y,
            std::int32_t z, random::RandomSource& random, std::int32_t age, std::vector<BlockChange>& changes) {
    const std::int32_t hardness = difficultyOf(difficulty);
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dy = -1; dy <= 4; ++dy) {
                if (dx == 0 && dy == 0 && dz == 0) {
                    continue;
                }
                std::int32_t rate = kBaseRate;
                if (dy > 1) {
                    rate += (dy - 1) * kBaseRate;
                }
                const std::int32_t tx = x + dx;
                const std::int32_t ty = y + dy;
                const std::int32_t tz = z + dz;
                const std::int32_t ignite = igniteOddsAround(world, tx, ty, tz);
                if (ignite <= 0) {
                    continue;
                }
                const std::int32_t odds = (ignite + kIgniteBonus + (hardness * kDifficultyWeight)) / (age + kAgeWeight);
                if (odds <= 0 || random.nextInt(rate) > odds) {
                    continue;
                }
                if (rainingOn(world, weather, tx, ty, tz)) {
                    continue;
                }
                const std::optional<std::int32_t> fire =
                    fireFor(world, tx, ty, tz, std::min(age + random.nextInt(5) / 4, kMaxAge));
                if (!fire.has_value()) {
                    continue;
                }
                put(world, tx, ty, tz, *fire, changes);
                schedule(world, tx, ty, tz, random);
            }
        }
    }
}

} // namespace

const wire::common::Identifier& tickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("fire");
    return instance;
}

std::int32_t tickDelay(random::RandomSource& random) { return kMinDelay + random.nextInt(kDelaySpread); }

bool isFire(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "fire";
}

void schedule(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random) {
    world.scheduleBlockTick(x, y, z, tickType(), tickDelay(random));
}

bool ignite(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
            const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen, bool hasSkylight) {
    const std::optional<std::int32_t> existing = world.blockState(x, y, z);
    if (!existing.has_value() || *existing != registry::blocks::air().stateId) {
        return false;
    }
    const std::optional<std::int32_t> fire = fireFor(world, x, y, z, 0);
    if (!fire.has_value()) {
        return false;
    }
    if (!blockSurvival::isSupported(world, x, y, z, *fire, scaffoldingHasFallen, hasSkylight)) {
        return false;
    }
    if (!world.setBlockState(x, y, z, *fire)) {
        return false;
    }
    schedule(world, x, y, z, random);
    return true;
}

std::vector<BlockChange> tick(WorldState& world, const WorldWeather& weather, std::string_view difficulty,
                              std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                              const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen, bool hasSkylight) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isFire(*stateId)) {
        return {};
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return {};
    }
    std::vector<BlockChange> changes;
    const std::int32_t age = ageOf(*stateId);

    if (!blockSurvival::isSupported(world, x, y, z, *stateId, scaffoldingHasFallen, hasSkylight)) {
        put(world, x, y, z, registry::blocks::air().stateId, changes);
        return changes;
    }
    // parity: rain puts a fire out on a roll that gets likelier as the fire ages.
    if (rainingOn(world, weather, x, y, z) &&
        random.nextFloat() < kRainBase + (static_cast<float>(age) * kRainPerAge)) {
        put(world, x, y, z, registry::blocks::air().stateId, changes);
        return changes;
    }

    const std::int32_t aged = std::min(kMaxAge, age + random.nextInt(3) / 2);
    if (aged != age) {
        if (const std::optional<std::int32_t> older =
                registry::blockStateDefinitions::stateIdOf(*block, {{"age", std::to_string(aged)}}, *stateId);
            older.has_value()) {
            put(world, x, y, z, *older, changes);
        }
    }
    schedule(world, x, y, z, random);

    if (!burnsForever(world, x, y, z)) {
        if (!hasSomethingToBurn(world, x, y, z)) {
            const std::int32_t below = stateAt(world, x, y - 1, z);
            if (!registry::blockShapes::isFaceSturdy(below, registry::BlockFace::Up) || aged > kOldEnoughToDie) {
                put(world, x, y, z, registry::blocks::air().stateId, changes);
            }
            return changes;
        }
        // parity: an old fire over something that will not burn goes out on a roll of four.
        if (aged == kMaxAge && random.nextInt(4) == 0 &&
            registry::fireOdds::burnOdds(stateAt(world, x, y - 1, z)) == 0) {
            put(world, x, y, z, registry::blocks::air().stateId, changes);
            return changes;
        }
    }

    burnOut(world, weather, x + 1, y, z, kSideChance, random, aged, changes);
    burnOut(world, weather, x - 1, y, z, kSideChance, random, aged, changes);
    burnOut(world, weather, x, y - 1, z, kVerticalChance, random, aged, changes);
    burnOut(world, weather, x, y + 1, z, kVerticalChance, random, aged, changes);
    burnOut(world, weather, x, y, z - 1, kSideChance, random, aged, changes);
    burnOut(world, weather, x, y, z + 1, kSideChance, random, aged, changes);
    spread(world, weather, difficulty, x, y, z, random, aged, changes);
    return changes;
}

} // namespace basalt::server::runtime::world::fires
