#include <basalt/server/registry/ItemProperties.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace basalt::server::registry::itemProperties {
namespace {

// parity: `Items`' `Item.Properties.rarity`, every other item is common.
constexpr std::array<std::pair<std::string_view, std::string_view>, 115> kRarities = {{
    {"angler_pottery_sherd", "uncommon"},
    {"archer_pottery_sherd", "uncommon"},
    {"arms_up_pottery_sherd", "uncommon"},
    {"barrier", "epic"},
    {"beacon", "rare"},
    {"blade_pottery_sherd", "uncommon"},
    {"bolt_armor_trim_smithing_template", "uncommon"},
    {"brewer_pottery_sherd", "uncommon"},
    {"burn_pottery_sherd", "uncommon"},
    {"chain_command_block", "epic"},
    {"chainmail_boots", "uncommon"},
    {"chainmail_chestplate", "uncommon"},
    {"chainmail_helmet", "uncommon"},
    {"chainmail_leggings", "uncommon"},
    {"coast_armor_trim_smithing_template", "uncommon"},
    {"command_block", "epic"},
    {"command_block_minecart", "epic"},
    {"conduit", "uncommon"},
    {"creeper_banner_pattern", "uncommon"},
    {"creeper_head", "uncommon"},
    {"danger_pottery_sherd", "uncommon"},
    {"debug_stick", "epic"},
    {"disc_fragment_5", "uncommon"},
    {"dragon_breath", "uncommon"},
    {"dragon_egg", "epic"},
    {"dragon_head", "epic"},
    {"dune_armor_trim_smithing_template", "uncommon"},
    {"echo_shard", "uncommon"},
    {"elytra", "epic"},
    {"enchanted_book", "rare"},
    {"enchanted_golden_apple", "rare"},
    {"experience_bottle", "uncommon"},
    {"explorer_pottery_sherd", "uncommon"},
    {"eye_armor_trim_smithing_template", "rare"},
    {"flow_armor_trim_smithing_template", "uncommon"},
    {"flow_banner_pattern", "rare"},
    {"flow_pottery_sherd", "uncommon"},
    {"friend_pottery_sherd", "uncommon"},
    {"goat_horn", "uncommon"},
    {"guster_banner_pattern", "rare"},
    {"guster_pottery_sherd", "uncommon"},
    {"heart_of_the_sea", "uncommon"},
    {"heart_pottery_sherd", "uncommon"},
    {"heartbreak_pottery_sherd", "uncommon"},
    {"heavy_core", "epic"},
    {"host_armor_trim_smithing_template", "uncommon"},
    {"howl_pottery_sherd", "uncommon"},
    {"jigsaw", "epic"},
    {"knowledge_book", "epic"},
    {"light", "epic"},
    {"mace", "epic"},
    {"miner_pottery_sherd", "uncommon"},
    {"mojang_banner_pattern", "rare"},
    {"mourner_pottery_sherd", "uncommon"},
    {"music_disc_11", "uncommon"},
    {"music_disc_13", "uncommon"},
    {"music_disc_5", "uncommon"},
    {"music_disc_blocks", "uncommon"},
    {"music_disc_bounce", "uncommon"},
    {"music_disc_cat", "uncommon"},
    {"music_disc_chirp", "uncommon"},
    {"music_disc_creator", "rare"},
    {"music_disc_creator_music_box", "uncommon"},
    {"music_disc_far", "uncommon"},
    {"music_disc_lava_chicken", "rare"},
    {"music_disc_mall", "uncommon"},
    {"music_disc_mellohi", "uncommon"},
    {"music_disc_otherside", "rare"},
    {"music_disc_pigstep", "rare"},
    {"music_disc_precipice", "uncommon"},
    {"music_disc_relic", "uncommon"},
    {"music_disc_stal", "uncommon"},
    {"music_disc_strad", "uncommon"},
    {"music_disc_tears", "uncommon"},
    {"music_disc_wait", "uncommon"},
    {"music_disc_ward", "uncommon"},
    {"nautilus_shell", "uncommon"},
    {"nether_star", "rare"},
    {"netherite_upgrade_smithing_template", "uncommon"},
    {"ominous_bottle", "uncommon"},
    {"piglin_banner_pattern", "uncommon"},
    {"piglin_head", "uncommon"},
    {"player_head", "uncommon"},
    {"plenty_pottery_sherd", "uncommon"},
    {"prize_pottery_sherd", "uncommon"},
    {"raiser_armor_trim_smithing_template", "uncommon"},
    {"recovery_compass", "uncommon"},
    {"repeating_command_block", "epic"},
    {"rib_armor_trim_smithing_template", "uncommon"},
    {"scrape_pottery_sherd", "uncommon"},
    {"sentry_armor_trim_smithing_template", "uncommon"},
    {"shaper_armor_trim_smithing_template", "uncommon"},
    {"sheaf_pottery_sherd", "uncommon"},
    {"shelter_pottery_sherd", "uncommon"},
    {"silence_armor_trim_smithing_template", "epic"},
    {"skeleton_skull", "uncommon"},
    {"skull_banner_pattern", "rare"},
    {"skull_pottery_sherd", "uncommon"},
    {"sniffer_egg", "uncommon"},
    {"snort_pottery_sherd", "uncommon"},
    {"snout_armor_trim_smithing_template", "uncommon"},
    {"spire_armor_trim_smithing_template", "rare"},
    {"structure_block", "epic"},
    {"structure_void", "epic"},
    {"test_block", "epic"},
    {"test_instance_block", "epic"},
    {"tide_armor_trim_smithing_template", "uncommon"},
    {"totem_of_undying", "uncommon"},
    {"trident", "rare"},
    {"vex_armor_trim_smithing_template", "rare"},
    {"ward_armor_trim_smithing_template", "rare"},
    {"wayfinder_armor_trim_smithing_template", "uncommon"},
    {"wild_armor_trim_smithing_template", "uncommon"},
    {"wither_skeleton_skull", "rare"},
    {"zombie_head", "uncommon"},
}};
constexpr std::array<std::string_view, 4> kArmourSuffixes{"_helmet", "_chestplate", "_leggings", "_boots"};
constexpr std::array<std::string_view, 6> kToolSuffixes{"_sword", "_shovel", "_pickaxe", "_axe", "_hoe", "_spear"};

// parity: `ToolMaterial.repairItems`.
constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kToolMaterials{{
    {"wooden_", "minecraft:wooden_tool_materials"},
    {"stone_", "minecraft:stone_tool_materials"},
    {"copper_", "minecraft:copper_tool_materials"},
    {"iron_", "minecraft:iron_tool_materials"},
    {"golden_", "minecraft:gold_tool_materials"},
    {"diamond_", "minecraft:diamond_tool_materials"},
    {"netherite_", "minecraft:netherite_tool_materials"},
}};

// parity: `ArmorMaterial.repairIngredient`.
constexpr std::array<std::pair<std::string_view, std::string_view>, 7> kArmourMaterials{{
    {"leather_", "minecraft:repairs_leather_armor"},
    {"copper_", "minecraft:repairs_copper_armor"},
    {"chainmail_", "minecraft:repairs_chain_armor"},
    {"iron_", "minecraft:repairs_iron_armor"},
    {"golden_", "minecraft:repairs_gold_armor"},
    {"diamond_", "minecraft:repairs_diamond_armor"},
    {"netherite_", "minecraft:repairs_netherite_armor"},
}};

[[nodiscard]] bool endsWithAny(std::string_view path, std::span<const std::string_view> suffixes) {
    return std::ranges::any_of(suffixes, [path](std::string_view suffix) { return path.ends_with(suffix); });
}

[[nodiscard]] std::optional<Repair>
byMaterial(std::string_view path, std::span<const std::pair<std::string_view, std::string_view>> materials) {
    for (const auto& [prefix, tag] : materials) {
        if (path.starts_with(prefix)) {
            return Repair{.target = std::string{tag}, .isTag = true};
        }
    }
    return std::nullopt;
}
} // namespace

std::string_view rarityOf(const wire::common::Identifier& item) {
    const auto found =
        std::ranges::lower_bound(kRarities, item.path(), {}, &std::pair<std::string_view, std::string_view>::first);
    return found != kRarities.end() && found->first == item.path() ? found->second : "common";
}

std::optional<Repair> repairableOf(const wire::common::Identifier& item) {
    const std::string_view path = item.path();
    if (path == "elytra") {
        return Repair{.target = "minecraft:phantom_membrane", .isTag = false};
    }
    if (path == "mace") {
        return Repair{.target = "minecraft:breeze_rod", .isTag = false};
    }
    if (path == "shield") {
        return Repair{.target = "minecraft:wooden_tool_materials", .isTag = true};
    }
    if (path == "turtle_helmet") {
        return Repair{.target = "minecraft:repairs_turtle_helmet", .isTag = true};
    }
    if (path == "wolf_armor") {
        return Repair{.target = "minecraft:repairs_wolf_armor", .isTag = true};
    }
    if (endsWithAny(path, kArmourSuffixes)) {
        return byMaterial(path, kArmourMaterials);
    }
    if (endsWithAny(path, kToolSuffixes)) {
        return byMaterial(path, kToolMaterials);
    }
    return std::nullopt;
}
} // namespace basalt::server::registry::itemProperties
