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

} // namespace

std::optional<std::string> schemaError(std::string_view registry, const JsonObject& element) {
    const auto& all = schemas();
    const auto found = std::ranges::find(all, registry, &Schema::registry);
    return found == all.end() ? std::nullopt : fields(element, found->fields);
}

} // namespace basalt::server::runtime::datapack
