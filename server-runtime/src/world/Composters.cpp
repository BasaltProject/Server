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

/// parity: `ComposterBlock.bootStrap`, item by item and chance by chance. It is a table in the jar's
/// own code rather than in a datapack, so it is a table here too.
constexpr std::array<std::pair<std::string_view, float>, 115> kCompostables{{
    {"acacia_leaves", 0.3F},
    {"acacia_sapling", 0.3F},
    {"allium", 0.65F},
    {"apple", 0.65F},
    {"azalea", 0.65F},
    {"azalea_leaves", 0.3F},
    {"azure_bluet", 0.65F},
    {"baked_potato", 0.85F},
    {"beetroot", 0.65F},
    {"beetroot_seeds", 0.3F},
    {"big_dripleaf", 0.65F},
    {"birch_leaves", 0.3F},
    {"birch_sapling", 0.3F},
    {"blue_orchid", 0.65F},
    {"bread", 0.85F},
    {"brown_mushroom", 0.65F},
    {"brown_mushroom_block", 0.85F},
    {"bush", 0.3F},
    {"cactus", 0.5F},
    {"cactus_flower", 0.3F},
    {"cake", 1.0F},
    {"carrot", 0.65F},
    {"carved_pumpkin", 0.65F},
    {"cherry_leaves", 0.3F},
    {"cherry_sapling", 0.3F},
    {"closed_eyeblossom", 0.65F},
    {"cocoa_beans", 0.65F},
    {"cookie", 0.85F},
    {"cornflower", 0.65F},
    {"crimson_fungus", 0.65F},
    {"crimson_roots", 0.65F},
    {"dandelion", 0.65F},
    {"dark_oak_leaves", 0.3F},
    {"dark_oak_sapling", 0.3F},
    {"dried_kelp", 0.3F},
    {"dried_kelp_block", 0.5F},
    {"dry_short_grass", 0.3F},
    {"dry_tall_grass", 0.3F},
    {"fern", 0.65F},
    {"firefly_bush", 0.3F},
    {"flowering_azalea", 0.85F},
    {"flowering_azalea_leaves", 0.5F},
    {"glow_berries", 0.3F},
    {"glow_lichen", 0.5F},
    {"hanging_roots", 0.3F},
    {"hay_block", 0.85F},
    {"jungle_leaves", 0.3F},
    {"jungle_sapling", 0.3F},
    {"kelp", 0.3F},
    {"large_fern", 0.65F},
    {"leaf_litter", 0.3F},
    {"lilac", 0.65F},
    {"lily_of_the_valley", 0.65F},
    {"lily_pad", 0.65F},
    {"mangrove_leaves", 0.3F},
    {"mangrove_propagule", 0.3F},
    {"mangrove_roots", 0.3F},
    {"melon", 0.65F},
    {"melon_seeds", 0.3F},
    {"melon_slice", 0.5F},
    {"moss_block", 0.65F},
    {"moss_carpet", 0.3F},
    {"mushroom_stem", 0.65F},
    {"nether_sprouts", 0.5F},
    {"nether_wart", 0.65F},
    {"nether_wart_block", 0.85F},
    {"oak_leaves", 0.3F},
    {"oak_sapling", 0.3F},
    {"open_eyeblossom", 0.65F},
    {"orange_tulip", 0.65F},
    {"oxeye_daisy", 0.65F},
    {"pale_hanging_moss", 0.3F},
    {"pale_moss_block", 0.65F},
    {"pale_moss_carpet", 0.3F},
    {"pale_oak_leaves", 0.3F},
    {"pale_oak_sapling", 0.3F},
    {"peony", 0.65F},
    {"pink_petals", 0.3F},
    {"pink_tulip", 0.65F},
    {"pitcher_plant", 0.85F},
    {"pitcher_pod", 0.3F},
    {"poppy", 0.65F},
    {"potato", 0.65F},
    {"pumpkin", 0.65F},
    {"pumpkin_pie", 1.0F},
    {"pumpkin_seeds", 0.3F},
    {"red_mushroom", 0.65F},
    {"red_mushroom_block", 0.85F},
    {"red_tulip", 0.65F},
    {"rose_bush", 0.65F},
    {"sea_pickle", 0.65F},
    {"seagrass", 0.3F},
    {"short_grass", 0.3F},
    {"shroomlight", 0.65F},
    {"small_dripleaf", 0.3F},
    {"spore_blossom", 0.65F},
    {"spruce_leaves", 0.3F},
    {"spruce_sapling", 0.3F},
    {"sugar_cane", 0.5F},
    {"sunflower", 0.65F},
    {"sweet_berries", 0.3F},
    {"tall_grass", 0.5F},
    {"torchflower", 0.85F},
    {"torchflower_seeds", 0.3F},
    {"twisting_vines", 0.5F},
    {"vine", 0.5F},
    {"warped_fungus", 0.65F},
    {"warped_roots", 0.65F},
    {"warped_wart_block", 0.85F},
    {"weeping_vines", 0.5F},
    {"wheat", 0.65F},
    {"wheat_seeds", 0.3F},
    {"white_tulip", 0.65F},
    {"wildflowers", 0.3F},
    {"wither_rose", 0.65F},
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

} // namespace

bool isComposter(std::int32_t stateId) { return pathOf(stateId) == "composter"; }

std::optional<float> chanceOf(const wire::common::Identifier& item) {
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
    const std::optional<float> chance = chanceOf(item);
    if (!level.has_value() || *level >= kFull || !chance.has_value()) {
        return std::nullopt;
    }
    // parity: the first layer goes in for anything compostable at all; every layer after it is a
    // roll, and the roll is `nextDouble` against the item's own chance.
    if (*level != 0 && random.nextDouble() >= static_cast<double>(*chance)) {
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
