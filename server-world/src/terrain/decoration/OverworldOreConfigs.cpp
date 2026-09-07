#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreConfigs.hpp>
#include <basalt/server/world/terrain/decoration/StoneBlockTags.hpp>

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

[[nodiscard]] Count fixedCount(std::int32_t value) {
    return [value](random::RandomSource&) { return value; };
}

[[nodiscard]] Count uniformCount(std::int32_t min, std::int32_t max) {
    return [min, max](random::RandomSource& random) { return min + random.nextInt(max - min + 1); };
}

[[nodiscard]] Count rarityCount(std::int32_t chance) {
    return [chance](random::RandomSource& random) {
        return random.nextFloat() < 1.0F / static_cast<float>(chance) ? 1 : 0;
    };
}

[[nodiscard]] Height uniformHeight(std::int32_t min, std::int32_t max) {
    return [min, max](random::RandomSource& random) { return min + random.nextInt(max - min + 1); };
}

[[nodiscard]] Height triangleHeight(std::int32_t min, std::int32_t max) {
    return [min, max](random::RandomSource& random) {
        const std::int32_t range = max - min;
        const std::int32_t firstHalf = range / 2;
        const std::int32_t secondHalf = range - firstHalf;
        return min + random.nextInt(secondHalf + 1) + random.nextInt(firstHalf + 1);
    };
}

const BlockSet& baseStoneOverworld() { return stoneBlockTags::baseStoneOverworld(); }

const BlockSet& stoneOreReplaceables() { return stoneBlockTags::stoneOreReplaceables(); }

const BlockSet& deepslateOreReplaceables() { return stoneBlockTags::deepslateOreReplaceables(); }

const BlockSet& mountainBiomes() {
    static const BlockSet blocks = {"windswept_hills",  "windswept_gravelly_hills",
                                    "windswept_forest", "cherry_grove",
                                    "meadow",           "grove",
                                    "snowy_slopes",     "frozen_peaks",
                                    "jagged_peaks",     "stony_peaks"};
    return blocks;
}

const BlockSet& badlandsBiomes() {
    static const BlockSet blocks = {"badlands", "wooded_badlands", "eroded_badlands"};
    return blocks;
}

/// The single-biome and all-but-one lists the table needs, interned so a PlacedOre can point at one.
const BlockSet& oneBiome(std::string_view name) {
    static std::map<std::string, BlockSet, std::less<>> interned;
    const auto found = interned.find(name);
    if (found != interned.end()) {
        return found->second;
    }
    return interned.emplace(std::string{name}, BlockSet{std::string{name}}).first->second;
}

const BlockSet& allBiomesExcept(std::string_view name) {
    static std::map<std::string, BlockSet, std::less<>> interned;
    const auto found = interned.find(name);
    if (found != interned.end()) {
        return found->second;
    }
    BlockSet built = allBiomes();
    built.erase(built.find(name));
    return interned.emplace(std::string{name}, std::move(built)).first->second;
}

[[nodiscard]] ConfiguredOre singleTarget(std::int32_t size, float discard, std::string_view block) {
    return ConfiguredOre{size, discard, {Target{&baseStoneOverworld(), stateIdOf(block)}}};
}

[[nodiscard]] ConfiguredOre pairedTarget(std::int32_t size, float discard, std::string_view stoneBlock,
                                         std::string_view deepslateBlock) {
    return ConfiguredOre{size,
                         discard,
                         {Target{&stoneOreReplaceables(), stateIdOf(stoneBlock)},
                          Target{&deepslateOreReplaceables(), stateIdOf(deepslateBlock)}}};
}

} // namespace

const BlockSet& allBiomes() {
    static const BlockSet blocks = {"plains",
                                    "sunflower_plains",
                                    "desert",
                                    "windswept_hills",
                                    "windswept_gravelly_hills",
                                    "windswept_forest",
                                    "windswept_savanna",
                                    "forest",
                                    "flower_forest",
                                    "birch_forest",
                                    "old_growth_birch_forest",
                                    "dark_forest",
                                    "old_growth_pine_taiga",
                                    "old_growth_spruce_taiga",
                                    "taiga",
                                    "snowy_taiga",
                                    "savanna",
                                    "savanna_plateau",
                                    "badlands",
                                    "wooded_badlands",
                                    "eroded_badlands",
                                    "jungle",
                                    "sparse_jungle",
                                    "bamboo_jungle",
                                    "swamp",
                                    "mangrove_swamp",
                                    "river",
                                    "frozen_river",
                                    "beach",
                                    "snowy_beach",
                                    "stony_shore",
                                    "snowy_plains",
                                    "ice_spikes",
                                    "ocean",
                                    "deep_ocean",
                                    "warm_ocean",
                                    "lukewarm_ocean",
                                    "deep_lukewarm_ocean",
                                    "cold_ocean",
                                    "deep_cold_ocean",
                                    "frozen_ocean",
                                    "deep_frozen_ocean",
                                    "mushroom_fields",
                                    "cherry_grove",
                                    "pale_garden",
                                    "meadow",
                                    "grove",
                                    "snowy_slopes",
                                    "frozen_peaks",
                                    "jagged_peaks",
                                    "stony_peaks",
                                    "dripstone_caves",
                                    "lush_caves",
                                    "deep_dark",
                                    "sulfur_caves"};
    return blocks;
}

const BlockSet& defaultDiskBiomes() {
    static const BlockSet blocks = [] {
        BlockSet built = allBiomes();
        built.erase("swamp");
        built.erase("mangrove_swamp");
        return built;
    }();
    return blocks;
}

const std::map<std::int32_t, PlacedOre>& oreFeaturesByIndex() {
    static const std::map<std::int32_t, PlacedOre> features = {
        {0, PlacedOre{0, fixedCount(7), uniformHeight(0, 160), &allBiomes(), singleTarget(33, 0.0F, "dirt")}},
        {1, PlacedOre{1, fixedCount(14), uniformHeight(-64, 319), &allBiomes(), singleTarget(33, 0.0F, "gravel")}},
        {2, PlacedOre{2, rarityCount(6), uniformHeight(64, 128), &allBiomes(), singleTarget(64, 0.0F, "granite")}},
        {3, PlacedOre{3, fixedCount(2), uniformHeight(0, 60), &allBiomes(), singleTarget(64, 0.0F, "granite")}},
        {4, PlacedOre{4, rarityCount(6), uniformHeight(64, 128), &allBiomes(), singleTarget(64, 0.0F, "diorite")}},
        {5, PlacedOre{5, fixedCount(2), uniformHeight(0, 60), &allBiomes(), singleTarget(64, 0.0F, "diorite")}},
        {6, PlacedOre{6, rarityCount(6), uniformHeight(64, 128), &allBiomes(), singleTarget(64, 0.0F, "andesite")}},
        {7, PlacedOre{7, fixedCount(2), uniformHeight(0, 60), &allBiomes(), singleTarget(64, 0.0F, "andesite")}},
        {8, PlacedOre{8, fixedCount(2), uniformHeight(-64, 0), &allBiomes(), singleTarget(64, 0.0F, "tuff")}},
        {9, PlacedOre{9, fixedCount(30), uniformHeight(136, 319), &allBiomes(),
                      pairedTarget(17, 0.0F, "coal_ore", "deepslate_coal_ore")}},
        {10, PlacedOre{10, fixedCount(20), triangleHeight(0, 192), &allBiomes(),
                       pairedTarget(17, 0.5F, "coal_ore", "deepslate_coal_ore")}},
        {11, PlacedOre{11, fixedCount(90), triangleHeight(80, 384), &allBiomes(),
                       pairedTarget(9, 0.0F, "iron_ore", "deepslate_iron_ore")}},
        {12, PlacedOre{12, fixedCount(10), triangleHeight(-24, 56), &allBiomes(),
                       pairedTarget(9, 0.0F, "iron_ore", "deepslate_iron_ore")}},
        {13, PlacedOre{13, fixedCount(10), uniformHeight(-64, 72), &allBiomes(),
                       pairedTarget(4, 0.0F, "iron_ore", "deepslate_iron_ore")}},
        {14, PlacedOre{14, fixedCount(4), triangleHeight(-64, 32), &allBiomes(),
                       pairedTarget(9, 0.5F, "gold_ore", "deepslate_gold_ore")}},
        {15, PlacedOre{15, uniformCount(0, 1), uniformHeight(-64, -48), &allBiomes(),
                       pairedTarget(9, 0.5F, "gold_ore", "deepslate_gold_ore")}},
        {16, PlacedOre{16, fixedCount(4), uniformHeight(-64, 15), &allBiomes(),
                       pairedTarget(8, 0.0F, "redstone_ore", "deepslate_redstone_ore")}},
        {17, PlacedOre{17, fixedCount(8), triangleHeight(-96, -32), &allBiomes(),
                       pairedTarget(8, 0.0F, "redstone_ore", "deepslate_redstone_ore")}},
        {18, PlacedOre{18, fixedCount(7), triangleHeight(-144, 16), &allBiomes(),
                       pairedTarget(4, 0.5F, "diamond_ore", "deepslate_diamond_ore")}},
        {19, PlacedOre{19, fixedCount(2), uniformHeight(-64, -4), &allBiomes(),
                       pairedTarget(8, 0.5F, "diamond_ore", "deepslate_diamond_ore")}},
        {20, PlacedOre{20, rarityCount(9), triangleHeight(-144, 16), &allBiomes(),
                       pairedTarget(12, 0.7F, "diamond_ore", "deepslate_diamond_ore")}},
        {21, PlacedOre{21, fixedCount(4), triangleHeight(-144, 16), &allBiomes(),
                       pairedTarget(8, 1.0F, "diamond_ore", "deepslate_diamond_ore")}},
        {22, PlacedOre{22, fixedCount(2), triangleHeight(-32, 32), &allBiomes(),
                       pairedTarget(7, 0.0F, "lapis_ore", "deepslate_lapis_ore")}},
        {23, PlacedOre{23, fixedCount(4), uniformHeight(-64, 64), &allBiomes(),
                       pairedTarget(7, 1.0F, "lapis_ore", "deepslate_lapis_ore")}},
        {24, PlacedOre{24, fixedCount(16), triangleHeight(-16, 112), &oneBiome("dripstone_caves"),
                       pairedTarget(20, 0.0F, "copper_ore", "deepslate_copper_ore")}},
        {25, PlacedOre{25, fixedCount(16), triangleHeight(-16, 112), &allBiomesExcept("dripstone_caves"),
                       pairedTarget(10, 0.0F, "copper_ore", "deepslate_copper_ore")}},
        {27, PlacedOre{27, fixedCount(46), uniformHeight(-64, 256), &oneBiome("lush_caves"),
                       singleTarget(33, 0.0F, "clay")}},
        {28, PlacedOre{28, fixedCount(50), uniformHeight(32, 256), &badlandsBiomes(),
                       pairedTarget(9, 0.0F, "gold_ore", "deepslate_gold_ore")}},
        {33, PlacedOre{33, fixedCount(100), triangleHeight(-16, 480), &mountainBiomes(),
                       pairedTarget(3, 0.0F, "emerald_ore", "deepslate_emerald_ore")}},
    };
    return features;
}

const PlacedOre& infestedOre() {
    // parity: the ten biomes that really list ore_infested.
    static const BlockSet biomes = {"cherry_grove",   "frozen_peaks",     "grove",
                                    "jagged_peaks",   "meadow",           "snowy_slopes",
                                    "stony_peaks",    "windswept_forest", "windswept_gravelly_hills",
                                    "windswept_hills"};
    static const PlacedOre ore{kInfestedGlobalIndex, fixedCount(14), uniformHeight(-64, 63), &biomes,
                               pairedTarget(9, 0.0F, "infested_stone", "infested_deepslate")};
    return ore;
}

} // namespace basalt::server::world::terrain::decoration
