#pragma once

#include <basalt/server/runtime/player/PlayerVersusPlayer.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::runtime::player {

namespace mace {

inline constexpr double kSmashThreshold = 1.5;

[[nodiscard]] bool isMace(const std::optional<storage::StoredItemStack>& weapon);

/// parity: `MaceItem.canSmashAttack`.
[[nodiscard]] bool canSmash(const PlayerSession& attacker);

[[nodiscard]] bool smashes(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon);

[[nodiscard]] float damageBonus(const PlayerSession& attacker, const std::optional<storage::StoredItemStack>& weapon,
                                const world::weaponDamage::DefinitionOf& definitionOf);

struct Struck {
    std::int32_t entityId = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool onGround = false;
    std::string uuid;
};

void hurtEnemy(const pvp::Context& context, world::LoadedWorld& world, PlayerSession& attacker, const Struck& struck);

void postHurtEnemy(PlayerSession& attacker);

[[nodiscard]] float armorFractionAdd(const std::optional<storage::StoredItemStack>& weapon,
                                     const world::weaponDamage::DefinitionOf& definitionOf);

} // namespace mace

} // namespace basalt::server::runtime::player
