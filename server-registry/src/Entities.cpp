#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/VanillaEntries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <utility>

namespace basalt::server::registry {
namespace {

using wire::common::Identifier;

[[nodiscard]] Identifier vanilla(std::string_view path) { return Identifier::vanilla(std::string{path}); }

/// The names travel with and without the namespace, because a plugin will write both.
[[nodiscard]] std::string_view unqualified(std::string_view name) {
    const std::size_t colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

} // namespace

namespace entityAttributes {
namespace {

[[nodiscard]] const std::unordered_map<Identifier, std::map<std::string, double>>& table() {
    static const std::unordered_map<Identifier, std::map<std::string, double>> built = [] {
        std::unordered_map<Identifier, std::map<std::string, double>> table;
        for (const EntityAttributeEntry& entry : entityAttributesTable()) {
            table[vanilla(entry.entity)].emplace(std::string{entry.attribute}, entry.value);
        }
        return table;
    }();
    return built;
}

} // namespace

std::optional<double> baseValue(const Identifier& type, std::string_view attribute) {
    const auto entity = table().find(type);
    if (entity == table().end()) {
        return std::nullopt;
    }
    const auto found = entity->second.find(std::string{unqualified(attribute)});
    return found == entity->second.end() ? std::nullopt : std::optional{found->second};
}

const std::map<std::string, double>& of(const Identifier& type) {
    static const std::map<std::string, double> none;
    const auto found = table().find(type);
    return found == table().end() ? none : found->second;
}

bool isLiving(const Identifier& type) { return table().contains(type); }

} // namespace entityAttributes

namespace attributeIds {
namespace {

/// parity: the `minecraft:attribute` registry's own order in the jar's report.
constexpr std::array<std::string_view, 40> kNames = {
    "air_drag_modifier",
    "armor",
    "armor_toughness",
    "attack_damage",
    "attack_knockback",
    "attack_speed",
    "below_name_distance",
    "block_break_speed",
    "block_interaction_range",
    "bounciness",
    "burning_time",
    "camera_distance",
    "explosion_knockback_resistance",
    "entity_interaction_range",
    "fall_damage_multiplier",
    "flying_speed",
    "follow_range",
    "friction_modifier",
    "gravity",
    "jump_strength",
    "knockback_resistance",
    "luck",
    "max_absorption",
    "max_health",
    "mining_efficiency",
    "movement_efficiency",
    "movement_speed",
    "name_tag_distance",
    "oxygen_bonus",
    "safe_fall_distance",
    "scale",
    "sneaking_speed",
    "spawn_reinforcements",
    "step_height",
    "submerged_mining_speed",
    "sweeping_damage_ratio",
    "tempt_range",
    "water_movement_efficiency",
    "waypoint_transmit_range",
    "waypoint_receive_range",
};

} // namespace

std::optional<std::int32_t> protocolId(std::string_view name) {
    const auto found = std::ranges::find(kNames, unqualified(name));
    return found == kNames.end() ? std::nullopt : std::optional{static_cast<std::int32_t>(found - kNames.begin())};
}

bool exists(std::string_view name) { return protocolId(name).has_value(); }

std::vector<std::string_view> all() {
    std::vector<std::string_view> names{kNames.begin(), kNames.end()};
    std::ranges::sort(names);
    return names;
}

} // namespace attributeIds

namespace brewing {
namespace {

/// Whether one of the comma-separated ingredients of a mix is this item.
[[nodiscard]] bool mentions(std::string_view ingredients, std::string_view item) {
    while (!ingredients.empty()) {
        const std::size_t comma = ingredients.find(',');
        if (ingredients.substr(0, comma) == item) {
            return true;
        }
        if (comma == std::string_view::npos) {
            return false;
        }
        ingredients = ingredients.substr(comma + 1);
    }
    return false;
}

} // namespace

const Identifier& fuel() {
    static const Identifier item = Identifier::vanilla("blaze_powder");
    return item;
}

bool isIngredient(const Identifier& item) {
    return std::ranges::any_of(brewingMixesTable(),
                               [&item](const BrewingMix& mix) { return mentions(mix.ingredients, item.path()); });
}

std::optional<Brew> mix(const Identifier& bottle, const std::optional<std::string>& potion,
                        const Identifier& ingredient) {
    for (const BrewingMix& one : brewingMixesTable()) {
        // The bottle itself changing: a potion into a splash potion.
        if (one.kind != "containerMixes" || one.from != bottle.path() ||
            !mentions(one.ingredients, ingredient.path())) {
            continue;
        }
        return Brew{vanilla(one.to), potion};
    }
    if (!potion.has_value()) {
        return std::nullopt;
    }
    for (const BrewingMix& one : brewingMixesTable()) {
        // What is inside it changing, the bottle staying as it is.
        if (one.kind != "potionMixes" || one.from != *potion || !mentions(one.ingredients, ingredient.path())) {
            continue;
        }
        return Brew{bottle, std::string{one.to}};
    }
    return std::nullopt;
}

} // namespace brewing

namespace entityTypeShapes {
namespace {

/// What an unknown type is assumed to fill, which is the size of a slime ball of nothing.
constexpr float kDefaultSize = 1.0F;

[[nodiscard]] const std::unordered_map<Identifier, Shape>& table() {
    static const std::unordered_map<Identifier, Shape> built = [] {
        std::unordered_map<Identifier, Shape> table;
        for (const EntityTypeShape& entry : entityTypeShapesTable()) {
            table.emplace(vanilla(entry.entity),
                          Shape{entry.entity, entry.width, entry.height, entry.category, entry.trackingRange,
                                entry.updateInterval, entry.fireImmune, entry.farFromPlayer});
        }
        return table;
    }();
    return built;
}

/// parity: the cube mobs carry a builder-only safety scale on the box a spawn is checked against.
[[nodiscard]] double spawnDimensionsScale(const Identifier& type) {
    if (type.path() == "magma_cube" || type.path() == "slime") {
        return 4.0;
    }
    return type.path() == "sulfur_cube" ? 2.0 : 1.0;
}

} // namespace

const Shape* of(const Identifier& type) {
    const auto found = table().find(type);
    return found == table().end() ? nullptr : &found->second;
}

double widthOf(const Identifier& type) {
    const Shape* shape = of(type);
    return static_cast<double>(shape == nullptr ? kDefaultSize : shape->width);
}

double heightOf(const Identifier& type) {
    const Shape* shape = of(type);
    return static_cast<double>(shape == nullptr ? kDefaultSize : shape->height);
}

double spawnWidthOf(const Identifier& type) { return widthOf(type) * spawnDimensionsScale(type); }

double spawnHeightOf(const Identifier& type) { return heightOf(type) * spawnDimensionsScale(type); }

std::string_view categoryOf(const Identifier& type) {
    const Shape* shape = of(type);
    return shape == nullptr ? "misc" : shape->category;
}

bool isFireImmune(const Identifier& type) {
    const Shape* shape = of(type);
    return shape != nullptr && shape->fireImmune;
}

bool isImmuneTo(const Identifier& type, std::string_view block) {
    const std::string_view path = type.path();
    if (path == "fox") {
        return block == "sweet_berry_bush";
    }
    if (path == "polar_bear" || path == "snow_golem" || path == "stray") {
        return block == "powder_snow";
    }
    if (path == "wither" || path == "wither_skeleton") {
        return block == "wither_rose";
    }
    return false;
}

bool canSpawnFarFromPlayer(const Identifier& type) {
    const Shape* shape = of(type);
    return shape == nullptr || shape->farFromPlayer;
}

} // namespace entityTypeShapes

} // namespace basalt::server::registry
