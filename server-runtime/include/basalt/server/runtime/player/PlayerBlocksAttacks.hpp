#pragma once

#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player::blocksAttacks {

struct HolderSet {
    std::vector<std::string> entries;
};

struct DamageReduction {
    float horizontalBlockingAngle = 90.0F;
    std::optional<HolderSet> type;
    float base = 0.0F;
    float factor = 1.0F;
};

struct ItemDamageFunction {
    float threshold = 1.0F;
    float base = 0.0F;
    float factor = 1.0F;
};

struct Component {
    float blockDelaySeconds = 0.0F;
    float disableCooldownScale = 1.0F;
    std::vector<DamageReduction> damageReductions{DamageReduction{}};
    ItemDamageFunction itemDamage;
    std::optional<HolderSet> bypassedBy;
    std::optional<std::string> blockSound;
    std::optional<std::string> disableSound;
};

/// The stack's `blocks_attacks`: its own, else the shield default; empty when it has none.
[[nodiscard]] std::optional<Component> of(const storage::StoredItemStack& stack);

[[nodiscard]] bool contains(const HolderSet& set, const world::realDamageTypeTags::TagMembers* tags,
                            const std::optional<wire::common::Identifier>& damageType);

[[nodiscard]] std::int32_t blockDelayTicks(const Component& component);

[[nodiscard]] float resolveBlockedDamage(const Component& component, const world::realDamageTypeTags::TagMembers* tags,
                                         const std::optional<wire::common::Identifier>& damageType, float dealt,
                                         double angle);

[[nodiscard]] std::int32_t itemDamageFor(const Component& component, float blocked);

/// Ticks of cooldown the component gives for a disabling hit of `baseSeconds`.
[[nodiscard]] std::int32_t disableTicks(const Component& component, float baseSeconds);

} // namespace basalt::server::runtime::player::blocksAttacks
