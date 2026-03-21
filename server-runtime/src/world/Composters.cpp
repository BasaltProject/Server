#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Composters.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world::composters {
namespace {

constexpr std::int32_t kAlwaysChance = 100;

/// parity: each item's `minecraft:compostable` component, which `Items` sets in code, as the chance
/// in percent of the `compostable/<low|low_medium|medium|medium_high|always_add_one>` provider it
/// names: 30, 50, 65, 85 and 100.
constexpr std::array<std::pair<std::string_view, std::int32_t>, 122> kCompostables{{
    {"acacia_leaves", 30},
    {"acacia_sapling", 30},
    {"allium", 65},
    {"apple", 65},
    {"azalea", 65},
    {"azalea_leaves", 30},
    {"azure_bluet", 65},
    {"baked_potato", 85},
    {"beetroot", 65},
    {"beetroot_seeds", 30},
    {"big_dripleaf", 65},
    {"birch_leaves", 30},
    {"birch_sapling", 30},
    {"blue_orchid", 65},
    {"bread", 85},
    {"brown_mushroom", 65},
    {"brown_mushroom_block", 85},
    {"bush", 30},
    {"cactus", 50},
    {"cactus_flower", 30},
    {"cake", 100},
    {"carrot", 65},
    {"carved_pumpkin", 65},
    {"cherry_leaves", 30},
    {"cherry_sapling", 30},
    {"closed_eyeblossom", 65},
    {"cocoa_beans", 65},
    {"cookie", 85},
    {"cornflower", 65},
    {"crimson_fungus", 65},
    {"crimson_roots", 65},
    {"dandelion", 65},
    {"dark_oak_leaves", 30},
    {"dark_oak_sapling", 30},
    {"dried_kelp", 30},
    {"dried_kelp_block", 50},
    {"fern", 65},
    {"firefly_bush", 30},
    {"flowering_azalea", 85},
    {"flowering_azalea_leaves", 50},
    {"glow_berries", 30},
    {"glow_lichen", 50},
    {"hanging_roots", 30},
    {"hay_block", 85},
    {"jungle_leaves", 30},
    {"jungle_sapling", 30},
    {"kelp", 30},
    {"large_fern", 65},
    {"leaf_litter", 30},
    {"lilac", 65},
    {"lily_of_the_valley", 65},
    {"lily_pad", 65},
    {"mangrove_leaves", 30},
    {"mangrove_propagule", 30},
    {"mangrove_roots", 30},
    {"melon", 65},
    {"melon_seeds", 30},
    {"melon_slice", 50},
    {"moss_block", 65},
    {"moss_carpet", 30},
    {"mushroom_stem", 65},
    {"nether_sprouts", 50},
    {"nether_wart", 65},
    {"nether_wart_block", 85},
    {"oak_leaves", 30},
    {"oak_sapling", 30},
    {"open_eyeblossom", 65},
    {"orange_poplar_leaves", 30},
    {"orange_tulip", 65},
    {"oxeye_daisy", 65},
    {"pale_hanging_moss", 30},
    {"pale_moss_block", 65},
    {"pale_moss_carpet", 30},
    {"pale_oak_leaves", 30},
    {"pale_oak_sapling", 30},
    {"peony", 65},
    {"pink_petals", 30},
    {"pink_tulip", 65},
    {"pitcher_plant", 85},
    {"pitcher_pod", 30},
    {"poplar_sapling", 30},
    {"poppy", 65},
    {"potato", 65},
    {"pumpkin", 65},
    {"pumpkin_pie", 100},
    {"pumpkin_seeds", 30},
    {"red_mushroom", 65},
    {"red_mushroom_block", 85},
    {"red_poplar_leaves", 30},
    {"red_shrub", 30},
    {"red_tulip", 65},
    {"rose_bush", 65},
    {"sea_pickle", 65},
    {"seagrass", 30},
    {"shelf_mushroom", 65},
    {"short_dry_grass", 30},
    {"short_grass", 30},
    {"shroomlight", 65},
    {"small_dripleaf", 30},
    {"spore_blossom", 65},
    {"spruce_leaves", 30},
    {"spruce_sapling", 30},
    {"straw_bed", 85},
    {"sugar_cane", 50},
    {"sunflower", 65},
    {"sweet_berries", 30},
    {"tall_dry_grass", 30},
    {"tall_grass", 50},
    {"torchflower", 85},
    {"torchflower_seeds", 30},
    {"twisting_vines", 50},
    {"vine", 50},
    {"warped_fungus", 65},
    {"warped_roots", 65},
    {"warped_wart_block", 85},
    {"weeping_vines", 50},
    {"wheat", 65},
    {"wheat_seeds", 30},
    {"white_tulip", 65},
    {"wildflowers", 30},
    {"wither_rose", 65},
    {"yellow_poplar_leaves", 30},
}};

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::optional<std::int32_t> levelOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("level");
    if (found == properties.end()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    const char* first = found->second.data();
    const char* last = first + found->second.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last ? std::optional{value} : std::nullopt;
}

[[nodiscard]] const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("composter");
    return instance;
}

/// Writes a new level, answering the change. Nothing when the state does not exist, which cannot
/// happen for a level the caller worked out from the one that is there.
[[nodiscard]] std::optional<BlockChange> fillTo(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                std::int32_t stateId, std::int32_t level) {
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(block(), {{"level", std::to_string(level)}}, stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

/// parity: the `compostable/*` context providers `ComposterBlock.addLayer` evaluates, written out
/// here because the composter has no reach into the jar: always one for `always_add_one` or an empty
/// composter (the `match_block` case, no draw), else one `nextInt(100)` against a two-entry weighted
/// list that gives one layer when the draw falls under the chance. 26.2 drew `nextDouble` instead.
[[nodiscard]] std::int32_t layersFor(std::int32_t level, std::int32_t chance, random::RandomSource& random) {
    if (chance >= kAlwaysChance || level == 0) {
        return 1;
    }
    return random.nextInt(kAlwaysChance) < chance ? 1 : 0;
}

} // namespace

bool isComposter(std::int32_t stateId) { return pathOf(stateId) == "composter"; }

std::optional<std::int32_t> chanceOf(const wire::common::Identifier& item) {
    if (item.namespaceName() != wire::common::Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const auto found =
        std::ranges::find_if(kCompostables, [&item](const auto& one) { return one.first == item.path(); });
    return found == kCompostables.end() ? std::nullopt : std::optional{found->second};
}

std::optional<std::int32_t> levelAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isComposter(*stateId)) {
        return std::nullopt;
    }
    return levelOf(*stateId);
}

std::optional<BlockChange> addItem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const wire::common::Identifier& item, random::RandomSource& random) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isComposter(*stateId)) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> level = levelOf(*stateId);
    const std::optional<std::int32_t> chance = chanceOf(item);
    if (!level.has_value() || *level >= kFull || !chance.has_value()) {
        return std::nullopt;
    }
    if (layersFor(*level, *chance, random) <= 0) {
        return std::nullopt;
    }
    const std::optional<BlockChange> filled = fillTo(world, x, y, z, *stateId, *level + 1);
    if (filled.has_value() && *level + 1 == kFull) {
        world.scheduleBlockTick(x, y, z, block(), kReadyDelay);
    }
    return filled;
}

std::optional<BlockChange> settle(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isComposter(*stateId) || levelOf(*stateId) != kFull) {
        return std::nullopt;
    }
    return fillTo(world, x, y, z, *stateId, kReady);
}

std::optional<BlockChange> empty(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !isComposter(*stateId) || levelOf(*stateId) != kReady) {
        return std::nullopt;
    }
    return fillTo(world, x, y, z, *stateId, 0);
}

} // namespace basalt::server::runtime::world::composters
