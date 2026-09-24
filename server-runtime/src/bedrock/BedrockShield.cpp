#include <basalt/server/runtime/bedrock/BedrockShield.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <optional>
#include <string_view>

namespace basalt::server::runtime::bedrock::shield {
namespace {

using player::PlayerInventory;

constexpr std::string_view kShield = "minecraft:shield";

[[nodiscard]] bool holdsShield(const player::PlayerSession& player, std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    return held.has_value() && held->id == kShield;
}

/// The hand a sneak raises: the main hand first, then the off hand, as the use it stands for.
[[nodiscard]] std::optional<std::int32_t> shieldHand(const player::PlayerSession& player) {
    const std::int32_t main = PlayerInventory::kHotbarStart + player.inventory.heldSlot();
    if (holdsShield(player, main)) {
        return main;
    }
    if (holdsShield(player, PlayerInventory::kOffhandSlot)) {
        return PlayerInventory::kOffhandSlot;
    }
    return std::nullopt;
}

} // namespace

void tick(player::PlayerSession& player, const std::int64_t gameTime) {
    const std::optional<std::int32_t> hand = player.sneaking && !player.dead ? shieldHand(player) : std::nullopt;
    if (player.blockingSlot.has_value() && hand != player.blockingSlot) {
        player.blockingSlot.reset();
        player.blockingTicks = 0;
    }
    if (!hand.has_value()) {
        return;
    }
    if (player.blockingSlot.has_value()) {
        ++player.blockingTicks;
        return;
    }
    // parity: `ItemCooldowns.isOnCooldown`: an axe's blow keeps the shield down until it runs out.
    if (player::onCooldownFor(player, wire::common::Identifier::vanilla("shield"), gameTime)) {
        return;
    }
    player.blockingSlot = *hand;
    player.blockingTicks = 0;
}

} // namespace basalt::server::runtime::bedrock::shield
