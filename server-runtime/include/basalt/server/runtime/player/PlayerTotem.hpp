#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerRegistry;
class PlayerSession;

/// parity: the `minecraft:death_protection` component, the totem's own default included: the
/// effects that run when it spends itself, or nothing for a stack that does not protect.
[[nodiscard]] std::optional<std::vector<server::registry::ConsumeEffect>>
deathProtectionOf(const storage::StoredItemStack& stack);

/// What saving a player from death needs from around them.
struct TotemContext {
    PlayerRegistry* registry = nullptr;
    /// Whether the killing blow's damage type carries `bypasses_invulnerability`.
    bool bypassesInvulnerability = false;
    std::function<float()> nextFloat;
    /// Told the slot that changed, however the player's edition says it.
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    /// Fires `used_totem` with the stack that was spent.
    std::function<void(PlayerSession& player, const storage::StoredItemStack& spent)> usedTotem;
    /// Raises `ITEM_INTERACT_FINISH` where the player is.
    std::function<void(const PlayerSession& player)> interactFinished;
};

/// parity: `LivingEntity.checkTotemDeathProtection`: the main hand then the off hand, one of the
/// stack spent, the statistic and the trigger, health back to one, the effects, and event 35 for
/// everybody watching. Answers whether the player was saved.
bool checkTotemDeathProtection(PlayerSession& player, const TotemContext& context);

} // namespace basalt::server::runtime::player
