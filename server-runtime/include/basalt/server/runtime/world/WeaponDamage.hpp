#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `EnchantmentHelper.modifyDamage`.
namespace weaponDamage {

using DefinitionOf = std::function<const wire::nbt::NbtCompound*(const wire::common::Identifier&)>;

struct Victim {
    wire::common::Identifier type;
    std::function<bool(const wire::common::Identifier& tag)> inTag;
};

[[nodiscard]] float of(const std::optional<storage::StoredItemStack>& stack, float base,
                       const DefinitionOf& definitionOf, const Victim* victim = nullptr);

/// parity: `EnchantmentHelper.runIterationOnItem`.
[[nodiscard]] float valueEffect(const storage::StoredItemStack& stack, std::string_view component, float base,
                                const DefinitionOf& definitionOf);

/// parity: `EnchantmentHelper.pickHighestLevel` over a list-per-level component.
[[nodiscard]] std::optional<std::string>
highestLevelEntry(const storage::StoredItemStack& stack, std::string_view component, const DefinitionOf& definitionOf);

} // namespace weaponDamage

namespace vanillaItemEnchantability {

[[nodiscard]] std::optional<std::int32_t> valueOf(std::string_view item);

} // namespace vanillaItemEnchantability

} // namespace basalt::server::runtime::world
