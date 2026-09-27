#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/datapack/ReloadableElements.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/server/world/advancement/AdvancementJson.hpp>
#include <basalt/server/world/crafting/RecipeJson.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

/// parity: the ids of the dispatch registries 26.3 registers; naming anything else fails.
constexpr std::array<std::string_view, 42> kLootFunctions{"apply_bonus",
                                                          "copy_components",
                                                          "copy_custom_data",
                                                          "copy_name",
                                                          "copy_state",
                                                          "discard",
                                                          "enchant_randomly",
                                                          "enchant_with_levels",
                                                          "enchanted_count_increase",
                                                          "exploration_map",
                                                          "explosion_decay",
                                                          "fill_player_head",
                                                          "filtered",
                                                          "furnace_smelt",
                                                          "limit_count",
                                                          "modify_contents",
                                                          "sequence",
                                                          "set_attributes",
                                                          "set_banner_pattern",
                                                          "set_book_cover",
                                                          "set_components",
                                                          "set_contents",
                                                          "set_count",
                                                          "set_custom_data",
                                                          "set_custom_model_data",
                                                          "set_damage",
                                                          "set_enchantments",
                                                          "set_firework_explosion",
                                                          "set_fireworks",
                                                          "set_instrument",
                                                          "set_item",
                                                          "set_loot_table",
                                                          "set_lore",
                                                          "set_name",
                                                          "set_ominous_bottle_amplifier",
                                                          "set_potion",
                                                          "set_random_dyes",
                                                          "set_random_potion",
                                                          "set_stew_effect",
                                                          "set_writable_book_pages",
                                                          "set_written_book_pages",
                                                          "toggle_tooltips"};
constexpr std::array<std::string_view, 20> kLootConditions{"all_of",
                                                           "any_of",
                                                           "damage_source_properties",
                                                           "enchantment_active_check",
                                                           "entity_properties",
                                                           "entity_scores",
                                                           "environment_attribute_check",
                                                           "float_value_check",
                                                           "int_value_check",
                                                           "inverted",
                                                           "killed_by_player",
                                                           "location_check",
                                                           "match_block",
                                                           "match_tool",
                                                           "random_chance",
                                                           "random_chance_with_enchanted_bonus",
                                                           "survives_explosion",
                                                           "table_bonus",
                                                           "time_check",
                                                           "weather_check"};
constexpr std::array<std::string_view, 9> kPoolEntries{"alternatives", "dynamic",  "empty", "group", "item",
                                                       "loot_table",   "sequence", "slots", "tag"};
constexpr std::array<std::string_view, 22> kRecipeSerializers{"blasting",
                                                              "brewing",
                                                              "campfire_cooking",
                                                              "crafting_decorated_pot",
                                                              "crafting_dye",
                                                              "crafting_imbue",
                                                              "crafting_shaped",
                                                              "crafting_shapeless",
                                                              "crafting_special_bannerduplicate",
                                                              "crafting_special_bookcloning",
                                                              "crafting_special_firework_rocket",
                                                              "crafting_special_firework_star",
                                                              "crafting_special_firework_star_fade",
                                                              "crafting_special_mapextending",
                                                              "crafting_special_repairitem",
                                                              "crafting_special_shielddecoration",
                                                              "crafting_transmute",
                                                              "smelting",
                                                              "smithing_transform",
                                                              "smithing_trim",
                                                              "smoking",
                                                              "stonecutting"};
constexpr std::array<std::string_view, 58> kTriggers{"allay_drop_item_on_block",
                                                     "any_block_use",
                                                     "avoid_vibration",
                                                     "bee_nest_destroyed",
                                                     "bred_animals",
                                                     "brewed_potion",
                                                     "changed_dimension",
                                                     "channeled_lightning",
                                                     "construct_beacon",
                                                     "consume_item",
                                                     "crafter_recipe_crafted",
                                                     "cured_zombie_villager",
                                                     "default_block_use",
                                                     "effects_changed",
                                                     "enchanted_item",
                                                     "enter_block",
                                                     "entity_hurt_player",
                                                     "entity_killed_player",
                                                     "fall_after_explosion",
                                                     "fall_from_height",
                                                     "filled_bucket",
                                                     "fishing_rod_hooked",
                                                     "hero_of_the_village",
                                                     "impossible",
                                                     "inventory_changed",
                                                     "item_durability_changed",
                                                     "item_used_on_block",
                                                     "kill_mob_near_sculk_catalyst",
                                                     "killed_by_arrow",
                                                     "levitation",
                                                     "lightning_strike",
                                                     "location",
                                                     "nether_travel",
                                                     "placed_block",
                                                     "player_generates_container_loot",
                                                     "player_hurt_entity",
                                                     "player_interacted_with_entity",
                                                     "player_killed_entity",
                                                     "player_sheared_equipment",
                                                     "recipe_crafted",
                                                     "recipe_unlocked",
                                                     "ride_entity_in_lava",
                                                     "shot_crossbow",
                                                     "slept_in_bed",
                                                     "slide_down_block",
                                                     "spear_mobs",
                                                     "started_riding",
                                                     "summoned_entity",
                                                     "tame_animal",
                                                     "target_hit",
                                                     "thrown_item_picked_up_by_entity",
                                                     "thrown_item_picked_up_by_player",
                                                     "tick",
                                                     "used_ender_eye",
                                                     "used_totem",
                                                     "using_item",
                                                     "villager_trade",
                                                     "voluntary_exile"};

template <std::size_t N>
[[nodiscard]] bool vanillaKnows(const std::array<std::string_view, N>& ids, std::string_view id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() && parsed->namespaceName() == "minecraft" &&
           std::ranges::find(ids, parsed->path()) != ids.end();
}

/// A reader's refusal is the pack's error unless it only names what this build has not modelled.
[[nodiscard]] std::optional<std::string> judged(const std::string& message) {
    constexpr std::array<std::pair<std::string_view, int>, 5> kGaps{{{"Unsupported loot function type: ", 0},
                                                                     {"Unsupported loot condition type: ", 1},
                                                                     {"Unsupported loot pool entry type: ", 2},
                                                                     {"Unsupported recipe type: ", 3},
                                                                     {"Unsupported ", 4}}};
    for (const auto& [prefix, which] : kGaps) {
        if (!message.starts_with(prefix)) {
            continue;
        }
        const std::string_view id = std::string_view{message}.substr(prefix.size());
        switch (which) {
        case 0:
            return vanillaKnows(kLootFunctions, id) ? std::nullopt : std::optional{message};
        case 1:
            return vanillaKnows(kLootConditions, id) ? std::nullopt : std::optional{message};
        case 2:
            return vanillaKnows(kPoolEntries, id) ? std::nullopt : std::optional{message};
        case 3:
            return vanillaKnows(kRecipeSerializers, id) ? std::nullopt : std::optional{message};
        default:
            return std::nullopt;
        }
    }
    return message;
}

[[nodiscard]] std::optional<std::string> advancementTriggers(std::string_view text) {
    const auto json = wire::common::parseJson(text);
    const auto* root = json.has_value() ? json->get<JsonObject>() : nullptr;
    const JsonValue* criteria = root == nullptr ? nullptr : root->find("criteria");
    const auto* object = criteria == nullptr ? nullptr : criteria->get<JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    for (const JsonValue& value : object->values()) {
        const auto* criterion = value.get<JsonObject>();
        const JsonValue* trigger = criterion == nullptr ? nullptr : criterion->find("trigger");
        const auto* name = trigger == nullptr ? nullptr : trigger->get<std::string>();
        if (name != nullptr && !vanillaKnows(kTriggers, *name)) {
            return "Unknown trigger: " + *name;
        }
    }
    return std::nullopt;
}

} // namespace

ItemTags itemTagsOf(const ResourceStack& resources) {
    return resolveTags([&resources](std::string_view prefix) { return resources.entriesUnder(prefix); },
                       [&resources](std::string_view name) { return resources.readText(name); }, "item",
                       [](const Identifier&) { return true; }, wire::common::Logger{"Datapacks"});
}

std::optional<std::string> reloadableElementError(std::string_view registry, std::string_view text,
                                                  const ResourceStack& resources, const ItemTags& itemTags) {
    namespace loot = server::world::loot;
    loot::LootReferences references([&resources](std::string_view folder,
                                                 const Identifier& id) -> std::optional<std::string> {
        return resources.readText("data/" + id.namespaceName() + "/" + std::string{folder} + "/" + id.path() + ".json");
    });
    const auto parsedJson = [&text]() { return wire::common::parseJson(text); };
    std::optional<std::string> error;
    if (registry == "loot_table") {
        if (auto parsed = loot::lootTableJson::parse(text, &references); !parsed.has_value()) {
            error = parsed.error().message;
        }
    } else if (registry == "predicate" || registry == "item_modifier" || registry == "context_int_provider" ||
               registry == "context_float_provider") {
        const auto json = parsedJson();
        if (!json.has_value()) {
            return std::nullopt;
        }
        if (registry == "predicate") {
            if (auto parsed = loot::lootTableJson::parseCondition(*json, &references); !parsed.has_value()) {
                error = parsed.error().message;
            }
        } else if (registry == "item_modifier") {
            if (auto parsed = loot::lootTableJson::parseModifier(*json, &references); !parsed.has_value()) {
                error = parsed.error().message;
            }
        } else if (registry == "context_int_provider") {
            if (auto parsed = loot::lootTableJson::parseIntProvider(*json, &references); !parsed.has_value()) {
                error = parsed.error().message;
            }
        } else if (auto parsed = loot::lootTableJson::parseFloatProvider(*json, &references); !parsed.has_value()) {
            error = parsed.error().message;
        }
    } else if (registry == "recipe") {
        const ItemTags& tags = itemTags;
        auto parsed = server::world::crafting::recipeJson::parse(text, [&tags](std::string_view tag) {
            std::set<std::string, std::less<>> items;
            const auto id = Identifier::parse(tag.starts_with('#') ? tag.substr(1) : tag);
            if (const auto found = id.has_value() ? tags.find(*id) : tags.end(); found != tags.end()) {
                for (const Identifier& item : found->second) {
                    items.insert(item.toString());
                }
            }
            return items;
        });
        if (!parsed.has_value()) {
            error = parsed.error().message;
        }
    } else if (registry == "advancement") {
        if (auto parsed = server::world::advancement::advancementJson::parse(text); !parsed.has_value()) {
            error = parsed.error().message;
        } else {
            return advancementTriggers(text);
        }
    }
    return error.has_value() ? judged(*error) : std::nullopt;
}

} // namespace basalt::server::runtime::datapack
