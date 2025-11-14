#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {

/// parity: `AttributeModifier.Operation`, whose serialized names are what `/attribute` takes.
enum class AttributeOperation : std::uint8_t { AddValue, AddMultipliedBase, AddMultipliedTotal };

[[nodiscard]] std::string_view attributeOperationName(AttributeOperation operation) noexcept;

[[nodiscard]] std::optional<AttributeOperation> attributeOperationByName(std::string_view name);

/// One modifier on one attribute. parity: `AttributeModifier`, keyed by id as 1.21 made it.
struct AttributeModifier {
    std::string id;
    double amount = 0.0;
    AttributeOperation operation = AttributeOperation::AddValue;

    friend bool operator==(const AttributeModifier&, const AttributeModifier&) = default;
};

/// The attribute ids commands name, as `minecraft:` ids. parity: `Attributes`.
namespace attribute {

inline constexpr std::string_view kMaxHealth = "minecraft:max_health";
inline constexpr std::string_view kMovementSpeed = "minecraft:movement_speed";
inline constexpr std::string_view kAttackDamage = "minecraft:attack_damage";
inline constexpr std::string_view kArmor = "minecraft:armor";
inline constexpr std::string_view kScale = "minecraft:scale";

/// parity: `Attributes`' own defaults for the ones a command is likely to touch.
[[nodiscard]] std::optional<double> defaultFor(std::string_view id);

} // namespace attribute

} // namespace basalt::server::runtime::entity
