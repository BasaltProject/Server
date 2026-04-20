#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::registry {
struct BlockMovementEntry {
    std::string_view block;
    float friction;
    float speedFactor;
    float jumpFactor;
    float bounciness;
};

struct BlockSoundTypeEntry {
    float volume;
    float pitch;
    std::string_view breakSound;
    std::string_view stepSound;
    std::string_view placeSound;
    std::string_view hitSound;
    std::string_view fallSound;
};

struct BlockSoundEntry {
    std::string_view block;
    std::int32_t soundType;
};

struct EntityAttributeEntry {
    std::string_view entity;
    std::string_view attribute;
    double value;
};

struct ItemDurabilityEntry {
    std::string_view item;
    std::int32_t maxDamage;
    std::int32_t damagePerBlock;
    std::int32_t damagePerAttack;
};

struct EntityTypeShape {
    std::string_view entity;
    float width;
    float height;
    std::string_view category;
    std::int32_t trackingRange;
    std::int32_t updateInterval;
    bool fireImmune;
    bool farFromPlayer;
};

struct ToolRule {
    std::string_view item;
    std::string_view blockTag;
    bool correctForDrops;
    std::optional<float> speed;
};

[[nodiscard]] std::span<const BlockMovementEntry> blockMovementTable();

[[nodiscard]] std::span<const BlockSoundTypeEntry> blockSoundTypesTable();

[[nodiscard]] std::span<const BlockSoundEntry> blockSoundsTable();

[[nodiscard]] std::span<const EntityAttributeEntry> entityAttributesTable();

[[nodiscard]] std::span<const ItemDurabilityEntry> itemDurabilityTable();

[[nodiscard]] std::span<const EntityTypeShape> entityTypeShapesTable();

[[nodiscard]] std::span<const ToolRule> itemToolsTable();
} // namespace basalt::server::registry
