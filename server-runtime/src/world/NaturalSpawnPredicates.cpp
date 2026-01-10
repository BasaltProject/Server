#include <basalt/server/runtime/world/NaturalSpawnPredicates.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace basalt::server::runtime::world::naturalSpawnPredicates {
namespace {

template <std::size_t N>
[[nodiscard]] bool contains(const std::array<std::string_view, N>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

constexpr auto kNoGroundPlacement = std::to_array<std::string_view>({"fox", "panda", "trader_llama"});

constexpr auto kArmadilloGround = std::to_array<std::string_view>(
    {"grass_block", "red_sand", "coarse_dirt", "terracotta", "white_terracotta", "yellow_terracotta",
     "orange_terracotta", "red_terracotta", "brown_terracotta", "light_gray_terracotta"});

constexpr auto kSandGround = std::to_array<std::string_view>({"sand", "red_sand", "suspicious_sand"});

constexpr auto kFoxWolfGround =
    std::to_array<std::string_view>({"grass_block", "snow", "snow_block", "podzol", "coarse_dirt"});

constexpr auto kFrogGround =
    std::to_array<std::string_view>({"grass_block", "mud", "mangrove_roots", "muddy_mangrove_roots"});

constexpr auto kGoatGround =
    std::to_array<std::string_view>({"grass_block", "stone", "snow", "snow_block", "packed_ice", "gravel"});

constexpr auto kRabbitGround = std::to_array<std::string_view>({"grass_block", "snow", "snow_block", "sand"});

constexpr auto kNetherStems = std::to_array<std::string_view>(
    {"crimson_stem", "stripped_crimson_stem", "crimson_hyphae", "stripped_crimson_hyphae", "warped_stem",
     "stripped_warped_stem", "warped_hyphae", "stripped_warped_hyphae"});

constexpr auto kPolarBearAlternateBiomes = std::to_array<std::string_view>({"frozen_ocean", "deep_frozen_ocean"});

constexpr auto kSurfaceMonsters = std::to_array<std::string_view>({"camel_husk", "husk", "parched", "stray"});

/// parity: a turtle needs the beach, which is four blocks either side of the sea.
constexpr std::int32_t kTurtleHeight = 4;

/// parity: `GlowSquid.checkGlowSquidSpawnRules`, thirty-three blocks under the sea.
constexpr std::int32_t kGlowSquidDepth = 33;

} // namespace

bool usesGroundPlacement(std::string_view type) { return !contains(kNoGroundPlacement, type); }

bool allows(std::string_view type, std::string_view ground, bool groundIsLeaves, std::string_view biome, std::int32_t y,
            std::int32_t seaLevel) {
    if (type == "armadillo") {
        return contains(kArmadilloGround, ground);
    }
    if (type == "camel") {
        return contains(kSandGround, ground);
    }
    if (type == "fox" || type == "wolf") {
        return contains(kFoxWolfGround, ground);
    }
    if (type == "frog") {
        return contains(kFrogGround, ground);
    }
    if (type == "goat") {
        return contains(kGoatGround, ground);
    }
    if (type == "mooshroom") {
        return ground == "mycelium";
    }
    if (type == "parrot") {
        return ground == "grass_block" || ground == "air" || ground.ends_with("_log") || ground.ends_with("_wood") ||
               contains(kNetherStems, ground) || groundIsLeaves;
    }
    if (type == "polar_bear") {
        return contains(kPolarBearAlternateBiomes, biome) ? ground == "ice" : ground == "grass_block";
    }
    if (type == "rabbit") {
        return contains(kRabbitGround, ground);
    }
    if (type == "turtle") {
        return y < seaLevel + kTurtleHeight && contains(kSandGround, ground);
    }
    return ground == "grass_block";
}

bool isSurfaceMonster(std::string_view type) { return contains(kSurfaceMonsters, type); }

bool waterPlacementAllows(bool atHasWater, bool aboveConductsRedstone) { return atHasWater && !aboveConductsRedstone; }

bool glowSquidAllows(std::int32_t y, std::int32_t seaLevel, std::int32_t rawBrightness, bool atIsWaterBlock) {
    return y <= seaLevel - kGlowSquidDepth && rawBrightness == 0 && atIsWaterBlock;
}

bool guardianAllows(std::int32_t oneInTwentyRoll, std::string_view difficulty, bool atHasWater, bool belowHasWater,
                    const std::function<bool()>& canSeeSkyFromBelowWater) {
    return (oneInTwentyRoll == 0 || !canSeeSkyFromBelowWater()) && difficulty != "peaceful" && atHasWater &&
           belowHasWater;
}

} // namespace basalt::server::runtime::world::naturalSpawnPredicates
