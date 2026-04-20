#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace raiderEquipment {

[[nodiscard]] const wire::nbt::NbtCompound& ominousBannerComponents();

[[nodiscard]] bool isOminousBanner(const wire::common::Identifier& item, std::int32_t count,
                                   const wire::nbt::NbtCompound& components);

void wearOminousBanner(ServerEntity& raider);

[[nodiscard]] bool wearsOminousBanner(const ServerEntity& raider);

[[nodiscard]] bool canBeLeader(std::string_view type);

void holdInMainHand(ServerEntity& raider, std::string_view item, std::optional<std::string_view> enchantment = {},
                    std::int32_t level = 1);

} // namespace raiderEquipment

} // namespace basalt::server::runtime::entity
