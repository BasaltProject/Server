#pragma once

#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::potionEffects {

/// parity: `PotionContents.forEachEffect`.
/// its duration scaled and floored, never below a tick.
[[nodiscard]] std::vector<ActiveEffect> of(const wire::nbt::NbtCompound& potionContents, float durationScale);

/// Every potion in registry order. parity: `Potions`, water first.
[[nodiscard]] std::vector<wire::common::Identifier> ids();

/// The effects a named potion carries. parity: `Potions`, by the path of its id.
[[nodiscard]] std::vector<ActiveEffect> ofPotion(std::string_view potion);

} // namespace basalt::server::runtime::world::potionEffects
