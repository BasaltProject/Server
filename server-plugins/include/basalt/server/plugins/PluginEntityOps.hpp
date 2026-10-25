#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::server::plugins {

/// Same alternatives as `PluginValue`; a list crosses as strings and rows are flattened with a fixed stride.
using PluginEntityValue =
    std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<std::string>>;

enum class EntityValueKind : std::uint8_t { Bool, Int, Real, Text, List };

struct EntityPropertySpec {
    std::string_view name;
    EntityValueKind kind;
    bool writable;
};

inline constexpr std::array kEntityProperties{
    EntityPropertySpec{"health", EntityValueKind::Real, true},
    EntityPropertySpec{"love_cause", EntityValueKind::Text, true},
    EntityPropertySpec{"fire_ticks", EntityValueKind::Int, true},
    EntityPropertySpec{"frozen_ticks", EntityValueKind::Int, true},
    EntityPropertySpec{"air", EntityValueKind::Int, true},
    EntityPropertySpec{"max_air", EntityValueKind::Int, true},
    EntityPropertySpec{"fall_distance", EntityValueKind::Real, true},
    EntityPropertySpec{"no_gravity", EntityValueKind::Bool, true},
    EntityPropertySpec{"invulnerable", EntityValueKind::Bool, true},
    EntityPropertySpec{"glowing", EntityValueKind::Bool, true},
    EntityPropertySpec{"silent", EntityValueKind::Bool, true},
    EntityPropertySpec{"invisible", EntityValueKind::Bool, true},
    EntityPropertySpec{"custom_name_visible", EntityValueKind::Bool, true},
    EntityPropertySpec{"custom_name", EntityValueKind::Text, true},
    EntityPropertySpec{"persistent", EntityValueKind::Bool, true},
    EntityPropertySpec{"ticks_lived", EntityValueKind::Int, true},
    EntityPropertySpec{"tags", EntityValueKind::List, true},
    EntityPropertySpec{"vehicle", EntityValueKind::Int, false},
    EntityPropertySpec{"passengers", EntityValueKind::List, false},
    EntityPropertySpec{"max_health", EntityValueKind::Real, true},
    EntityPropertySpec{"absorption", EntityValueKind::Real, true},
    EntityPropertySpec{"no_ai", EntityValueKind::Bool, true},
    EntityPropertySpec{"can_pick_up_loot", EntityValueKind::Bool, true},
    EntityPropertySpec{"target", EntityValueKind::Int, true},
    EntityPropertySpec{"last_damage", EntityValueKind::Real, true},
    EntityPropertySpec{"last_damage_cause", EntityValueKind::Text, false},
    EntityPropertySpec{"no_damage_ticks", EntityValueKind::Int, true},
    EntityPropertySpec{"arrows_in_body", EntityValueKind::Int, true},
    EntityPropertySpec{"sleeping", EntityValueKind::Bool, false},
    EntityPropertySpec{"leash_holder", EntityValueKind::Int, true},
    EntityPropertySpec{"left_handed", EntityValueKind::Bool, true},
    EntityPropertySpec{"breed_age", EntityValueKind::Int, true},
    EntityPropertySpec{"love_ticks", EntityValueKind::Int, true},
    EntityPropertySpec{"owner", EntityValueKind::Text, true},
    EntityPropertySpec{"sitting", EntityValueKind::Bool, true},
    EntityPropertySpec{"owner_id", EntityValueKind::Int, true},
    EntityPropertySpec{"critical", EntityValueKind::Bool, true},
    EntityPropertySpec{"arrow_pickup", EntityValueKind::Int, true},
    EntityPropertySpec{"pierce_level", EntityValueKind::Int, true},
    EntityPropertySpec{"explosion_power", EntityValueKind::Real, true},
    EntityPropertySpec{"last_hurt_by_player", EntityValueKind::Text, true},
    EntityPropertySpec{"cube_size", EntityValueKind::Int, true},
    EntityPropertySpec{"fuse", EntityValueKind::Int, true},
    EntityPropertySpec{"powered", EntityValueKind::Bool, true},
    EntityPropertySpec{"swell", EntityValueKind::Int, true},
    EntityPropertySpec{"profession", EntityValueKind::Text, true},
    EntityPropertySpec{"trading_player", EntityValueKind::Int, false},
    EntityPropertySpec{"villager_level", EntityValueKind::Int, true},
    EntityPropertySpec{"villager_experience", EntityValueKind::Int, true},
    EntityPropertySpec{"tamed_horse", EntityValueKind::Bool, true},
    EntityPropertySpec{"aggressive", EntityValueKind::Bool, true},
    EntityPropertySpec{"age_locked", EntityValueKind::Bool, true},
    EntityPropertySpec{"burns_in_day", EntityValueKind::Bool, true},
    EntityPropertySpec{"despawn_in_peaceful", EntityValueKind::Int, true},
    EntityPropertySpec{"nav_open_doors", EntityValueKind::Bool, true},
    EntityPropertySpec{"nav_pass_doors", EntityValueKind::Bool, true},
    EntityPropertySpec{"nav_can_float", EntityValueKind::Bool, true},
    EntityPropertySpec{"bounce", EntityValueKind::Bool, true},
    EntityPropertySpec{"incendiary", EntityValueKind::Bool, true},
    EntityPropertySpec{"arrow_knockback", EntityValueKind::Int, true},
    EntityPropertySpec{"arrow_life", EntityValueKind::Int, true},
    EntityPropertySpec{"fired_from", EntityValueKind::Text, true},
    EntityPropertySpec{"bolt_cause", EntityValueKind::Int, true},
    EntityPropertySpec{"wither_charged", EntityValueKind::Bool, true},
    EntityPropertySpec{"trident_glint", EntityValueKind::Bool, true},
    EntityPropertySpec{"attached_to", EntityValueKind::Int, true},
    EntityPropertySpec{"eye_despawn", EntityValueKind::Int, true},
    EntityPropertySpec{"eye_drops", EntityValueKind::Bool, true},
};

struct EntityOperationSpec {
    std::string_view name;
    /// Comma separated column names when the operation returns a list of rows, empty otherwise.
    std::string_view columns;
};

inline constexpr std::array kEntityOperations{
    EntityOperationSpec{"damage", ""},
    EntityOperationSpec{"kill", ""},
    EntityOperationSpec{"add_effect", ""},
    EntityOperationSpec{"remove_effect", ""},
    EntityOperationSpec{"clear_effects", ""},
    EntityOperationSpec{"has_effect", ""},
    EntityOperationSpec{"effects", "effect,duration,amplifier,ambient,particles,icon"},
    EntityOperationSpec{"attribute", ""},
    EntityOperationSpec{"attribute_base", ""},
    EntityOperationSpec{"attribute_default", ""},
    EntityOperationSpec{"has_attribute", ""},
    EntityOperationSpec{"set_attribute_base", ""},
    EntityOperationSpec{"attribute_modifiers", "id,amount,operation"},
    EntityOperationSpec{"add_attribute_modifier", ""},
    EntityOperationSpec{"remove_attribute_modifier", ""},
    EntityOperationSpec{"equipment", ""},
    EntityOperationSpec{"set_equipment", ""},
    EntityOperationSpec{"drop_chance", ""},
    EntityOperationSpec{"set_drop_chance", ""},
    EntityOperationSpec{"mount", ""},
    EntityOperationSpec{"dismount", ""},
    EntityOperationSpec{"eject", ""},
    EntityOperationSpec{"merge_nbt", ""},
    EntityOperationSpec{"nbt", ""},
    EntityOperationSpec{"nbt_value", ""},
    EntityOperationSpec{"nbt_set", ""},
    EntityOperationSpec{"nbt_remove", ""},
    EntityOperationSpec{"push", ""},
    EntityOperationSpec{"knockback", ""},
    EntityOperationSpec{"hit_entity", ""},
    EntityOperationSpec{"hook_own", ""},
    EntityOperationSpec{"hook_retrieve", ""},
    EntityOperationSpec{"land", ""},
    EntityOperationSpec{"eye_target", ""},
    EntityOperationSpec{"set_eye_target", ""},
    EntityOperationSpec{"add_goal", ""},
    EntityOperationSpec{"remove_goal", ""},
    EntityOperationSpec{"remove_goals", ""},
    EntityOperationSpec{"has_goal", ""},
    EntityOperationSpec{"goals", "key,priority,running,plugin,flags"},
};

} // namespace basalt::server::plugins
