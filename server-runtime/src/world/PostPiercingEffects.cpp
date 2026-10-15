#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/world/PostPiercingEffects.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::world::postPiercing {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] std::optional<double> numberOf(const NbtTag* tag) {
    if (tag == nullptr) {
        return std::nullopt;
    }
    if (const auto* value = tag->get<double>(); value != nullptr) {
        return *value;
    }
    if (const auto* value = tag->get<float>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int32_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int64_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int16_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    if (const auto* value = tag->get<std::int8_t>(); value != nullptr) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

/// parity: `LevelBasedValue`, a constant or `linear`.
[[nodiscard]] float levelBased(const NbtTag* tag, std::int32_t level) {
    if (const std::optional<double> constant = numberOf(tag); constant.has_value()) {
        return static_cast<float>(*constant);
    }
    const auto* value = tag == nullptr ? nullptr : tag->get<NbtCompound>();
    if (value == nullptr) {
        return 0.0F;
    }
    const auto base = static_cast<float>(numberOf(value->find("base")).value_or(0.0));
    const auto perLevel = static_cast<float>(numberOf(value->find("per_level_above_first")).value_or(0.0));
    return base + (perLevel * static_cast<float>(level - 1));
}

[[nodiscard]] std::array<double, 3> vectorOf(const NbtTag* tag) {
    std::array<double, 3> out{};
    const auto* list = tag == nullptr ? nullptr : tag->get<NbtList>();
    if (list == nullptr || list->size() != 3) {
        return out;
    }
    for (std::size_t at = 0; at < 3; ++at) {
        out[at] = numberOf(&(*list)[at]).value_or(0.0);
    }
    return out;
}

[[nodiscard]] bool flagMatches(std::string_view flag, bool wanted, const User& user) {
    if (flag == "is_fall_flying") {
        return user.fallFlying == wanted;
    }
    if (flag == "is_in_water") {
        return user.inWater == wanted;
    }
    if (flag == "is_on_ground") {
        return user.onGround == wanted;
    }
    if (flag == "is_sneaking") {
        return user.sneaking == wanted;
    }
    if (flag == "is_sprinting") {
        return user.sprinting == wanted;
    }
    return false;
}

/// parity: `PlayerPredicate`, the gamemode list and the food level range.
[[nodiscard]] bool playerMatches(const NbtCompound& predicate, const User& user) {
    if (!user.player) {
        return false;
    }
    for (std::size_t at = 0; at < predicate.size(); ++at) {
        const std::string& key = predicate.names()[at];
        const NbtTag& value = predicate.values()[at];
        if (key == "gamemode") {
            const auto* modes = value.get<NbtList>();
            const auto* single = value.get<std::string>();
            bool any = single != nullptr && *single == user.gamemode;
            for (std::size_t index = 0; modes != nullptr && index < modes->size(); ++index) {
                const auto* mode = (*modes)[index].get<std::string>();
                any = any || (mode != nullptr && *mode == user.gamemode);
            }
            if (!any) {
                return false;
            }
        } else if (key == "food") {
            const auto* food = value.get<NbtCompound>();
            const NbtTag* level = food == nullptr ? nullptr : food->find("level");
            if (food == nullptr || food->size() != 1 || level == nullptr) {
                return false;
            }
            const auto foodLevel = static_cast<double>(user.food);
            if (const std::optional<double> exact = numberOf(level); exact.has_value()) {
                if (foodLevel != *exact) {
                    return false;
                }
            } else if (const auto* range = level->get<NbtCompound>(); range != nullptr) {
                const std::optional<double> min = numberOf(range->find("min"));
                const std::optional<double> max = numberOf(range->find("max"));
                if ((min.has_value() && foodLevel < *min) || (max.has_value() && foodLevel > *max)) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    return true;
}

/// The parts of `EntityPredicate` lunge asks about; anything else is unmet.
[[nodiscard]] bool entityMatches(const NbtCompound& predicate, const User& user) {
    for (std::size_t at = 0; at < predicate.size(); ++at) {
        const std::string& key = predicate.names()[at];
        const auto* inner = predicate.values()[at].get<NbtCompound>();
        if (inner == nullptr) {
            return false;
        }
        if (key == "minecraft:vehicle") {
            if (!user.riding || inner->size() != 0) {
                return false;
            }
        } else if (key == "minecraft:flags") {
            for (std::size_t flag = 0; flag < inner->size(); ++flag) {
                const auto* bit = inner->values()[flag].get<std::int8_t>();
                if (bit == nullptr || !flagMatches(inner->names()[flag], *bit != 0, user)) {
                    return false;
                }
            }
        } else if (key == "minecraft:type_specific/player") {
            if (!playerMatches(*inner, user)) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool meets(const NbtCompound* requirement, const User& user) {
    if (requirement == nullptr) {
        return true;
    }
    const std::string_view type = requirement->getString("type");
    if (type == "minecraft:all_of" || type == "minecraft:any_of") {
        const NbtList* terms = requirement->getList("terms");
        const bool all = type == "minecraft:all_of";
        if (terms == nullptr) {
            return all;
        }
        for (const NbtTag& term : terms->elements()) {
            const bool met = meets(term.get<NbtCompound>(), user) && term.get<NbtCompound>() != nullptr;
            if (met != all) {
                return met;
            }
        }
        return all;
    }
    if (type == "minecraft:inverted") {
        const NbtCompound* term = requirement->getCompound("term");
        return term != nullptr && !meets(term, user);
    }
    if (type == "minecraft:entity_properties") {
        const NbtCompound* predicate = requirement->getCompound("predicate");
        return requirement->getString("entity") == "this" && predicate != nullptr && entityMatches(*predicate, user);
    }
    return false;
}

/// parity: `ApplyEntityImpulse`.
[[nodiscard]] std::array<double, 3> impulseOf(const NbtCompound& effect, std::int32_t level, const User& user) {
    const std::array<double, 3> direction = vectorOf(effect.find("direction"));
    const std::array<double, 3> scale = vectorOf(effect.find("coordinate_scale"));
    const auto magnitude = static_cast<double>(levelBased(effect.find("magnitude"), level));
    const double pitch = static_cast<double>(user.pitch) * std::numbers::pi / 180.0;
    const double yaw = static_cast<double>(user.yaw) * std::numbers::pi / 180.0;
    // parity: `calculateViewQuaternion`.
    const double y = (direction[1] * std::cos(pitch)) - (direction[2] * std::sin(pitch));
    const double z0 = (direction[1] * std::sin(pitch)) + (direction[2] * std::cos(pitch));
    const double x = (direction[0] * std::cos(yaw)) - (z0 * std::sin(yaw));
    const double z = (direction[0] * std::sin(yaw)) + (z0 * std::cos(yaw));
    return {x * scale[0] * magnitude, y * scale[1] * magnitude, z * scale[2] * magnitude};
}

void apply(const NbtCompound& effect, std::int32_t level, const User& user, Outcome& outcome) {
    const std::string_view type = effect.getString("type");
    if (type == "minecraft:all_of") {
        const NbtList* effects = effect.getList("effects");
        for (std::size_t at = 0; effects != nullptr && at < effects->size(); ++at) {
            if (const auto* inner = (*effects)[at].get<NbtCompound>(); inner != nullptr) {
                apply(*inner, level, user, outcome);
            }
        }
    } else if (type == "minecraft:change_item_damage") {
        outcome.itemDamage += static_cast<std::int32_t>(levelBased(effect.find("amount"), level));
    } else if (type == "minecraft:apply_exhaustion") {
        if (user.player) {
            outcome.exhaustion += levelBased(effect.find("amount"), level);
        }
    } else if (type == "minecraft:apply_impulse") {
        const std::array<double, 3> push = impulseOf(effect, level, user);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            outcome.impulse[axis] += push[axis];
        }
        outcome.pushed = true;
    } else if (type == "minecraft:play_sound") {
        // parity: `PlaySoundEffect`, the entry for the level or the last one.
        const NbtTag* sound = effect.find("sound");
        std::optional<std::string> name;
        if (const auto* single = sound == nullptr ? nullptr : sound->get<std::string>(); single != nullptr) {
            name = *single;
        } else if (const auto* list = sound == nullptr ? nullptr : sound->get<NbtList>();
                   list != nullptr && !list->empty()) {
            const auto index =
                static_cast<std::size_t>(std::clamp(level - 1, 0, static_cast<std::int32_t>(list->size()) - 1));
            if (const auto* entry = (*list)[index].get<std::string>(); entry != nullptr) {
                name = *entry;
            }
        }
        if (name.has_value()) {
            outcome.sounds.push_back(Sound{.name = *name,
                                           .volume = static_cast<float>(numberOf(effect.find("volume")).value_or(1.0)),
                                           .pitch = static_cast<float>(numberOf(effect.find("pitch")).value_or(1.0))});
        }
    }
}

} // namespace

Outcome of(const storage::StoredItemStack& weapon, const weaponDamage::DefinitionOf& definitionOf, const User& user) {
    Outcome outcome;
    if (!definitionOf) {
        return outcome;
    }
    for (const auto& [id, level] : player::enchantmentLevels(weapon)) {
        const NbtCompound* definition = definitionOf(id);
        const NbtCompound* effects = definition == nullptr ? nullptr : definition->getCompound("effects");
        const NbtList* entries = effects == nullptr ? nullptr : effects->getList("minecraft:post_piercing_attack");
        for (std::size_t at = 0; entries != nullptr && at < entries->size(); ++at) {
            const auto* entry = (*entries)[at].get<NbtCompound>();
            const NbtCompound* effect = entry == nullptr ? nullptr : entry->getCompound("effect");
            if (effect != nullptr && meets(entry->getCompound("requirements"), user)) {
                apply(*effect, level, user, outcome);
            }
        }
    }
    return outcome;
}

} // namespace basalt::server::runtime::world::postPiercing
