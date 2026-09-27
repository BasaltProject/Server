#include <basalt/server/runtime/datapack/RegistrySchemas.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::common::JsonObject;
using wire::common::JsonValue;

enum class Kind : std::uint8_t {
    /// `Identifier.CODEC`.
    Id,
    String,
    Number,
    Bool,
    /// A holder set: an id, a `#tag`, or a list of ids.
    HolderSet,
    /// `IntProviders.CODEC`: a number or a typed provider.
    IntProvider,
    /// A string or a list of them.
    Strings,
    Object,
    /// `ComponentSerialization.CODEC`: a string, a list or an object.
    Text,
    /// `SoundEvent.CODEC`: an id, or an inline event.
    Sound,
    /// `ChatTypeDecoration.CODEC`.
    Decoration,
    /// `SpawnPrioritySelectors.CODEC`.
    SpawnConditions,
    /// `WolfVariant.AssetInfo.CODEC`.
    WolfAssets,
    /// A nested sound set, one sound per field.
    WolfSounds,
    PigSounds,
    CatSounds,
    ChickenSounds,
};

struct Field {
    std::string_view name;
    Kind kind;
};

struct Schema {
    std::string_view registry;
    std::vector<Field> fields;
};

constexpr std::string_view kVariantAsset = "asset_id";

const std::array<Schema, 21>& schemas() {
    static const std::array<Schema, 21> kSchemas{{
        {"damage_type", {{"message_id", Kind::String}, {"scaling", Kind::String}, {"exhaustion", Kind::Number}}},
        {"banner_pattern", {{"asset_id", Kind::Id}, {"translation_key", Kind::String}}},
        {"painting_variant", {{"width", Kind::Number}, {"height", Kind::Number}, {"asset_id", Kind::Id}}},
        {"trim_material", {{"palette_id", Kind::Id}, {"description", Kind::Text}}},
        {"trim_pattern", {{"asset_id", Kind::Id}, {"description", Kind::Text}}},
        {"jukebox_song",
         {{"sound_event", Kind::Sound},
          {"description", Kind::Text},
          {"length_in_seconds", Kind::Number},
          {"comparator_output", Kind::Number}}},
        {"instrument",
         {{"sound_event", Kind::Sound},
          {"use_duration", Kind::Number},
          {"range", Kind::Number},
          {"description", Kind::Text}}},
        {"chat_type", {{"chat", Kind::Decoration}, {"narration", Kind::Decoration}}},
        {"decorated_pot_pattern", {{"asset_id", Kind::Id}}},
        {"wolf_variant",
         {{"assets", Kind::WolfAssets},
          {"baby_assets", Kind::WolfAssets},
          {"spawn_conditions", Kind::SpawnConditions}}},
        {"wolf_sound_variant", {{"adult_sounds", Kind::WolfSounds}, {"baby_sounds", Kind::WolfSounds}}},
        {"pig_variant",
         {{kVariantAsset, Kind::Id}, {"baby_asset_id", Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
        {"pig_sound_variant", {{"adult_sounds", Kind::PigSounds}, {"baby_sounds", Kind::PigSounds}}},
        {"cat_variant",
         {{kVariantAsset, Kind::Id}, {"baby_asset_id", Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
        {"cat_sound_variant", {{"adult_sounds", Kind::CatSounds}, {"baby_sounds", Kind::CatSounds}}},
        {"cow_variant",
         {{kVariantAsset, Kind::Id}, {"baby_asset_id", Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
        {"cow_sound_variant",
         {{"ambient_sound", Kind::Sound},
          {"hurt_sound", Kind::Sound},
          {"death_sound", Kind::Sound},
          {"step_sound", Kind::Sound}}},
        {"chicken_variant",
         {{kVariantAsset, Kind::Id}, {"baby_asset_id", Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
        {"chicken_sound_variant", {{"adult_sounds", Kind::ChickenSounds}, {"baby_sounds", Kind::ChickenSounds}}},
        {"frog_variant", {{kVariantAsset, Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
        {"zombie_nautilus_variant", {{kVariantAsset, Kind::Id}, {"spawn_conditions", Kind::SpawnConditions}}},
    }};
    return kSchemas;
}

std::optional<std::string> check(const JsonValue& value, Kind kind);

std::optional<std::string> fields(const JsonObject& object, const std::vector<Field>& required) {
    for (const Field& field : required) {
        const JsonValue* value = object.find(field.name);
        if (value == nullptr) {
            return "No key " + std::string{field.name};
        }
        if (std::optional<std::string> error = check(*value, field.kind); error.has_value()) {
            return std::string{field.name} + ": " + *error;
        }
    }
    return std::nullopt;
}

std::optional<std::string> sounds(const JsonValue& value, std::initializer_list<std::string_view> names) {
    const auto* object = value.get<JsonObject>();
    if (object == nullptr) {
        return "Not a map";
    }
    for (const std::string_view name : names) {
        const JsonValue* sound = object->find(name);
        if (sound == nullptr) {
            return "No key " + std::string{name};
        }
        if (std::optional<std::string> error = check(*sound, Kind::Sound); error.has_value()) {
            return std::string{name} + ": " + *error;
        }
    }
    return std::nullopt;
}

/// A holder set: an id, a `#tag`, or a list of ids.
std::optional<std::string> holderSet(const JsonValue& value) {
    if (const auto* text = value.get<std::string>(); text != nullptr) {
        const std::string_view id =
            text->starts_with('#') ? std::string_view{*text}.substr(1) : std::string_view{*text};
        return Identifier::parse(id).has_value() ? std::nullopt
                                                 : std::optional<std::string>{"Not a valid id: " + *text};
    }
    const auto* list = value.get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return "Not a holder set";
    }
    for (const JsonValue& element : *list) {
        if (std::optional<std::string> error = check(element, Kind::Id); error.has_value()) {
            return error;
        }
    }
    return std::nullopt;
}

std::optional<std::string> spawnCondition(const JsonObject& condition) {
    const JsonValue* type = condition.find("type");
    const auto* name = type == nullptr ? nullptr : type->get<std::string>();
    const auto id =
        name == nullptr ? std::optional<Identifier>{} : Identifier::parse(*name).value_or(Identifier{"", ""});
    if (!id.has_value() || id->path().empty()) {
        return std::string{"No key type"};
    }
    const std::string kind = id->toString();
    if (kind == "minecraft:biome" || kind == "minecraft:structure") {
        const std::string_view key = kind == "minecraft:biome" ? "biomes" : "structures";
        const JsonValue* set = condition.find(key);
        return set == nullptr ? std::optional<std::string>{"No key " + std::string{key}} : holderSet(*set);
    }
    if (kind == "minecraft:moon_brightness") {
        const JsonValue* range = condition.find("range");
        if (range == nullptr) {
            return std::string{"No key range"};
        }
        return range->get<JsonObject>() != nullptr || check(*range, Kind::Number) == std::nullopt
                   ? std::nullopt
                   : std::optional<std::string>{"Not a range"};
    }
    return "Unknown spawn condition type: " + kind;
}

std::optional<std::string> spawnConditions(const JsonValue& value) {
    const auto* list = value.get<std::vector<JsonValue>>();
    if (list == nullptr) {
        return "Not a list";
    }
    for (const JsonValue& element : *list) {
        const auto* selector = element.get<JsonObject>();
        if (selector == nullptr) {
            return "Not a map";
        }
        const JsonValue* priority = selector->find("priority");
        if (priority == nullptr) {
            return "No key priority";
        }
        if (std::optional<std::string> error = check(*priority, Kind::Number); error.has_value()) {
            return "priority: " + *error;
        }
        if (const JsonValue* condition = selector->find("condition"); condition != nullptr) {
            const auto* object = condition->get<JsonObject>();
            if (object == nullptr) {
                return "condition: Not a map";
            }
            if (std::optional<std::string> error = spawnCondition(*object); error.has_value()) {
                return "condition: " + *error;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> check(const JsonValue& value, Kind kind) {
    switch (kind) {
    case Kind::Id: {
        const auto* text = value.get<std::string>();
        if (text == nullptr) {
            return "Not a string";
        }
        return Identifier::parse(*text).has_value() ? std::nullopt
                                                    : std::optional<std::string>{"Not a valid id: " + *text};
    }
    case Kind::String:
        return value.get<std::string>() != nullptr ? std::nullopt : std::optional<std::string>{"Not a string"};
    case Kind::Number:
        return value.get<std::int64_t>() != nullptr || value.get<double>() != nullptr
                   ? std::nullopt
                   : std::optional<std::string>{"Not a number"};
    case Kind::Bool:
        return value.get<bool>() != nullptr ? std::nullopt : std::optional<std::string>{"Not a boolean"};
    case Kind::HolderSet:
        return holderSet(value);
    case Kind::IntProvider:
        return value.get<JsonObject>() != nullptr ? std::nullopt : check(value, Kind::Number);
    case Kind::Strings: {
        if (value.get<std::string>() != nullptr) {
            return std::nullopt;
        }
        const auto* list = value.get<std::vector<JsonValue>>();
        if (list == nullptr) {
            return "Not a string or a list";
        }
        for (const JsonValue& element : *list) {
            if (element.get<std::string>() == nullptr) {
                return "Not a string";
            }
        }
        return std::nullopt;
    }
    case Kind::Object:
        return value.get<JsonObject>() != nullptr ? std::nullopt : std::optional<std::string>{"Not a map"};
    case Kind::Text:
        return value.isNull() ? std::optional<std::string>{"Not a component"} : std::nullopt;
    case Kind::Sound:
        if (const auto* inline_ = value.get<JsonObject>(); inline_ != nullptr) {
            return fields(*inline_, {{"sound_id", Kind::Id}});
        }
        return check(value, Kind::Id);
    case Kind::Decoration: {
        const auto* object = value.get<JsonObject>();
        if (object == nullptr) {
            return "Not a map";
        }
        const JsonValue* parameters = object->find("parameters");
        if (parameters == nullptr) {
            return "No key parameters";
        }
        if (parameters->get<std::vector<JsonValue>>() == nullptr) {
            return "parameters: Not a list";
        }
        return fields(*object, {{"translation_key", Kind::String}});
    }
    case Kind::SpawnConditions:
        return spawnConditions(value);
    case Kind::WolfAssets: {
        const auto* object = value.get<JsonObject>();
        return object == nullptr ? std::optional<std::string>{"Not a map"}
                                 : fields(*object, {{"wild", Kind::Id}, {"tame", Kind::Id}, {"angry", Kind::Id}});
    }
    case Kind::WolfSounds:
        return sounds(value, {"ambient_sound", "death_sound", "growl_sound", "hurt_sound", "pant_sound", "whine_sound",
                              "step_sound"});
    case Kind::PigSounds:
        return sounds(value, {"ambient_sound", "hurt_sound", "death_sound", "step_sound", "eat_sound"});
    case Kind::CatSounds:
        return sounds(value, {"ambient_sound", "stray_ambient_sound", "hiss_sound", "hurt_sound", "death_sound",
                              "eat_sound", "beg_for_food_sound", "purr_sound", "purreow_sound"});
    case Kind::ChickenSounds:
        return sounds(value, {"ambient_sound", "hurt_sound", "death_sound", "step_sound"});
    }
    return std::nullopt;
}

/// parity: the id lists of the dispatch registries, which a typed element must name.
constexpr std::array<std::string_view, 30> kEnchantmentEffects{"ammo_use",
                                                               "armor_effectiveness",
                                                               "attributes",
                                                               "block_experience",
                                                               "crossbow_charge_time",
                                                               "crossbow_charging_sounds",
                                                               "damage",
                                                               "damage_immunity",
                                                               "damage_protection",
                                                               "equipment_drops",
                                                               "fishing_luck_bonus",
                                                               "fishing_time_reduction",
                                                               "hit_block",
                                                               "item_damage",
                                                               "knockback",
                                                               "location_changed",
                                                               "mob_experience",
                                                               "post_attack",
                                                               "post_piercing_attack",
                                                               "prevent_armor_change",
                                                               "prevent_equipment_drop",
                                                               "projectile_count",
                                                               "projectile_piercing",
                                                               "projectile_spawned",
                                                               "projectile_spread",
                                                               "repair_with_xp",
                                                               "smash_damage_per_fallen_block",
                                                               "tick",
                                                               "trident_return_acceleration",
                                                               "trident_sound"};
constexpr std::string_view kTridentSpin = "trident_spin_attack_strength";

/// parity: every entity, location and value effect type an enchantment effect may name.
constexpr std::array<std::string_view, 23> kEffectTypes{"add",
                                                        "all_of",
                                                        "apply_exhaustion",
                                                        "apply_impulse",
                                                        "apply_mob_effect",
                                                        "attribute",
                                                        "change_item_damage",
                                                        "damage_entity",
                                                        "exponential",
                                                        "explode",
                                                        "ignite",
                                                        "multiply",
                                                        "play_sound",
                                                        "remove_binomial",
                                                        "replace_block",
                                                        "replace_disk",
                                                        "run_function",
                                                        "set",
                                                        "set_block_properties",
                                                        "spawn_particles",
                                                        "summon_entity",
                                                        "apply_impulse",
                                                        "all_of"};

[[nodiscard]] std::string vanillaPath(std::string_view id);

/// Every `effect` object inside the effects map must name a type vanilla registers.
std::optional<std::string> effectTypes(const JsonValue& value) {
    if (const auto* list = value.get<std::vector<JsonValue>>(); list != nullptr) {
        for (const JsonValue& element : *list) {
            if (auto error = effectTypes(element); error.has_value()) {
                return error;
            }
        }
        return std::nullopt;
    }
    const auto* object = value.get<JsonObject>();
    if (object == nullptr) {
        return std::nullopt;
    }
    if (const JsonValue* effect = object->find("effect"); effect != nullptr) {
        if (const auto* typed = effect->get<JsonObject>(); typed != nullptr) {
            const JsonValue* type = typed->find("type");
            const auto* name = type == nullptr ? nullptr : type->get<std::string>();
            if (name == nullptr) {
                return std::string{"effect: No key type"};
            }
            if (std::ranges::find(kEffectTypes, vanillaPath(*name)) == kEffectTypes.end()) {
                return "effect: Unknown effect type " + *name;
            }
            if (const JsonValue* inner = typed->find("effects"); inner != nullptr) {
                return effectTypes(*inner);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string vanillaPath(std::string_view id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() && parsed->namespaceName() == "minecraft" ? parsed->path() : std::string{};
}

std::optional<std::string> costs(const JsonObject& element) {
    for (const std::string_view key : {std::string_view{"min_cost"}, std::string_view{"max_cost"}}) {
        const JsonValue* cost = element.find(key);
        const auto* object = cost == nullptr ? nullptr : cost->get<JsonObject>();
        if (object == nullptr) {
            return cost == nullptr ? "No key " + std::string{key} : std::string{key} + ": Not a map";
        }
        if (auto error = fields(*object, {{"base", Kind::Number}, {"per_level_above_first", Kind::Number}});
            error.has_value()) {
            return std::string{key} + ": " + *error;
        }
    }
    return std::nullopt;
}

/// parity: `Enchantment.DIRECT_CODEC`; the effects are keyed by effect component type.
std::optional<std::string> enchantment(const JsonObject& element) {
    if (auto error = fields(element, {{"description", Kind::Text},
                                      {"supported_items", Kind::HolderSet},
                                      {"weight", Kind::Number},
                                      {"max_level", Kind::Number},
                                      {"anvil_cost", Kind::Number},
                                      {"slots", Kind::Strings}});
        error.has_value()) {
        return error;
    }
    if (auto error = costs(element); error.has_value()) {
        return error;
    }
    for (const std::string_view key : {std::string_view{"primary_items"}, std::string_view{"exclusive_set"}}) {
        if (const JsonValue* set = element.find(key); set != nullptr) {
            if (auto error = holderSet(*set); error.has_value()) {
                return std::string{key} + ": " + *error;
            }
        }
    }
    if (const JsonValue* effects = element.find("effects"); effects != nullptr) {
        const auto* object = effects->get<JsonObject>();
        if (object == nullptr) {
            return std::string{"effects: Not a map"};
        }
        for (const std::string& name : object->names()) {
            const std::string path = vanillaPath(name);
            if (path != kTridentSpin && std::ranges::find(kEnchantmentEffects, path) == kEnchantmentEffects.end()) {
                return "effects: Unknown effect component type " + name;
            }
        }
        for (const JsonValue& component : object->values()) {
            if (auto error = effectTypes(component); error.has_value()) {
                return "effects: " + *error;
            }
        }
    }
    return std::nullopt;
}

/// parity: `DimensionType.DIRECT_CODEC`.
std::optional<std::string> dimensionType(const JsonObject& element) {
    return fields(element, {{"has_skylight", Kind::Bool},
                            {"has_ceiling", Kind::Bool},
                            {"coordinate_scale", Kind::Number},
                            {"min_y", Kind::Number},
                            {"height", Kind::Number},
                            {"logical_height", Kind::Number},
                            {"infiniburn", Kind::String},
                            {"ambient_light", Kind::Number},
                            {"monster_spawn_light_level", Kind::IntProvider},
                            {"monster_spawn_block_light_limit", Kind::Number}});
}

/// parity: `Biome.DIRECT_CODEC`: climate, the effects' water colour, carvers and features.
std::optional<std::string> biome(const JsonObject& element) {
    if (auto error = fields(element, {{"has_precipitation", Kind::Bool},
                                      {"temperature", Kind::Number},
                                      {"downfall", Kind::Number},
                                      {"effects", Kind::Object}});
        error.has_value()) {
        return error;
    }
    if (element.find("effects")->get<JsonObject>()->find("water_color") == nullptr) {
        return std::string{"effects: No key water_color"};
    }
    for (const std::string_view key : {std::string_view{"carvers"}, std::string_view{"features"}}) {
        const JsonValue* value = element.find(key);
        if (value == nullptr) {
            return "No key " + std::string{key};
        }
        if (value->get<std::vector<JsonValue>>() == nullptr && value->get<std::string>() == nullptr) {
            return std::string{key} + ": Not a list";
        }
    }
    return std::nullopt;
}

std::optional<std::string> button(const JsonValue& value) {
    const auto* object = value.get<JsonObject>();
    return object == nullptr ? std::optional<std::string>{"Not a map"} : fields(*object, {{"label", Kind::Text}});
}

/// parity: `Dialog.DIRECT_CODEC`, dispatched on `type`, every one with a title.
std::optional<std::string> dialog(const JsonObject& element) {
    const JsonValue* type = element.find("type");
    const auto* name = type == nullptr ? nullptr : type->get<std::string>();
    if (name == nullptr) {
        return std::string{"No key type"};
    }
    const std::string kind = vanillaPath(*name);
    if (auto error = fields(element, {{"title", Kind::Text}}); error.has_value()) {
        return error;
    }
    const auto buttonAt = [&element](std::string_view key, bool required) -> std::optional<std::string> {
        const JsonValue* value = element.find(key);
        if (value == nullptr) {
            return required ? std::optional<std::string>{"No key " + std::string{key}} : std::nullopt;
        }
        auto error = button(*value);
        return error.has_value() ? std::optional<std::string>{std::string{key} + ": " + *error} : std::nullopt;
    };
    if (kind == "notice") {
        return buttonAt("action", false);
    }
    if (kind == "server_links") {
        return buttonAt("exit_action", false);
    }
    if (kind == "confirmation") {
        auto error = buttonAt("yes", true);
        return error.has_value() ? error : buttonAt("no", true);
    }
    if (kind == "multi_action") {
        const JsonValue* actions = element.find("actions");
        const auto* list = actions == nullptr ? nullptr : actions->get<std::vector<JsonValue>>();
        if (list == nullptr || list->empty()) {
            return std::string{"actions: Not a non-empty list"};
        }
        for (const JsonValue& action : *list) {
            if (auto error = button(action); error.has_value()) {
                return "actions: " + *error;
            }
        }
        return buttonAt("exit_action", false);
    }
    if (kind == "dialog_list") {
        const JsonValue* dialogs = element.find("dialogs");
        if (dialogs == nullptr) {
            return std::string{"No key dialogs"};
        }
        if (dialogs->get<JsonObject>() != nullptr || dialogs->get<std::vector<JsonValue>>() != nullptr) {
            return std::nullopt;
        }
        auto error = holderSet(*dialogs);
        return error.has_value() ? std::optional<std::string>{"dialogs: " + *error} : std::nullopt;
    }
    return "Unknown dialog type: " + *name;
}

} // namespace

std::optional<std::string> schemaError(std::string_view registry, const JsonObject& element) {
    if (registry == "enchantment") {
        return enchantment(element);
    }
    if (registry == "dimension_type") {
        return dimensionType(element);
    }
    if (registry == "worldgen/biome") {
        return biome(element);
    }
    if (registry == "dialog") {
        return dialog(element);
    }
    const auto& all = schemas();
    const auto found = std::ranges::find(all, registry, &Schema::registry);
    return found == all.end() ? std::nullopt : fields(element, found->fields);
}

} // namespace basalt::server::runtime::datapack
