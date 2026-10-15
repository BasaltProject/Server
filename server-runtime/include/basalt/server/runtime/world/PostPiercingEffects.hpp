#pragma once

#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `doPostPiercingAttackEffects` over `minecraft:post_piercing_attack`.
namespace postPiercing {

struct User {
    bool player = true;
    bool riding = false;
    bool fallFlying = false;
    bool inWater = false;
    bool onGround = false;
    bool sneaking = false;
    bool sprinting = false;
    std::string_view gamemode = "survival";
    std::int32_t food = 20;
    float yaw = 0.0F;
    float pitch = 0.0F;
};

struct Sound {
    std::string name;
    float volume = 1.0F;
    float pitch = 1.0F;
};

struct Outcome {
    std::int32_t itemDamage = 0;
    float exhaustion = 0.0F;
    bool pushed = false;
    std::array<double, 3> impulse{};
    std::vector<Sound> sounds;
};

[[nodiscard]] Outcome of(const storage::StoredItemStack& weapon, const weaponDamage::DefinitionOf& definitionOf,
                         const User& user);

} // namespace postPiercing

} // namespace basalt::server::runtime::world
