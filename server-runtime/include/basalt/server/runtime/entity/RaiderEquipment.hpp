#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a raider carries: the ominous banner a captain wears, and the weapon a raid hands out.
namespace raiderEquipment {

/// parity: `Raid.getBannerComponentPatch`, the components of a white banner that make it ominous.
[[nodiscard]] const wire::nbt::NbtCompound& ominousBannerComponents();

/// parity: `ItemStack.matches` against `Raid.getOminousBannerInstance`.
[[nodiscard]] bool isOminousBanner(const wire::common::Identifier& item, std::int32_t count,
                                   const wire::nbt::NbtCompound& components);

/// parity: `Raid.setLeader`'s helmet slot: the banner, dropped every time.
void wearOminousBanner(ServerEntity& raider);

[[nodiscard]] bool wearsOminousBanner(const ServerEntity& raider);

/// parity: `PatrollingMonster.canBeLeader`, which a witch and a ravager turn down.
[[nodiscard]] bool canBeLeader(std::string_view type);

/// Puts `item` in the main hand, with one enchantment when asked for.
void holdInMainHand(ServerEntity& raider, std::string_view item, std::optional<std::string_view> enchantment = {},
                    std::int32_t level = 1);

} // namespace raiderEquipment

} // namespace basalt::server::runtime::entity
