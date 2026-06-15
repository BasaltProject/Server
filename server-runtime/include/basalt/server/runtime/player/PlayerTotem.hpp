#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {

class PlayerRegistry;
class PlayerSession;

[[nodiscard]] std::optional<std::vector<server::registry::ConsumeEffect>>
deathProtectionOf(const storage::StoredItemStack& stack);

struct TotemContext {
    PlayerRegistry* registry = nullptr;
    bool bypassesInvulnerability = false;
    std::function<float()> nextFloat;
    std::function<void(const PlayerSession& player, std::int32_t slot)> slotChanged;
    std::function<void(PlayerSession& player, const storage::StoredItemStack& spent)> usedTotem;
    std::function<bool(const PlayerSession& player, bool offhand)> resurrectAllowed;
    std::function<void(const PlayerSession& player)> interactFinished;
};

bool checkTotemDeathProtection(PlayerSession& player, const TotemContext& context);

} // namespace basalt::server::runtime::player
