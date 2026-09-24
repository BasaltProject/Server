#include <basalt/server/runtime/bedrock/BedrockShield.hpp>
#include <basalt/server/runtime/player/PlayerCooldowns.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

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

/// Raises the shield in the hand a sneak lifts it with, keeps its count, or drops it.
void raise(player::PlayerSession& player, const std::int64_t gameTime) {
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

/// parity: `LivingEntity.DATA_LIVING_ENTITY_FLAGS`, using an item and with which hand.
void announce(const player::PlayerSession& player, const std::vector<player::PlayerSession*>& watchers) {
    constexpr std::int8_t kUsingItem = 1;
    constexpr std::int8_t kOffhand = 2;
    const bool raised = player.blockingSlot.has_value();
    const bool offhand = raised && *player.blockingSlot == PlayerInventory::kOffhandSlot;
    const wire::protocol::play::SetEntityDataPacket packet{
        .entityId = player.entityId(),
        .entries = {wire::protocol::play::SetEntityDataPacket::Entry{
            .index = wire::protocol::play::SetEntityDataPacket::kLivingFlagsIndex,
            .value = wire::protocol::play::SetEntityDataPacket::ByteValue{
                .value = static_cast<std::int8_t>((raised ? kUsingItem : 0) | (offhand ? kOffhand : 0))}}}};
    for (const player::PlayerSession* other : watchers) {
        if (other->entityId() != player.entityId() && other->worldName == player.worldName &&
            other->connection().sendPlayPacket) {
            other->connection().sendPlayPacket(wire::protocol::play::SetEntityDataPacket::kId,
                                               [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

} // namespace

void tick(player::PlayerSession& player, const std::int64_t gameTime,
          const std::vector<player::PlayerSession*>& watchers) {
    const bool wasRaised = player.blockingSlot.has_value();
    raise(player, gameTime);
    if (player.blockingSlot.has_value() != wasRaised) {
        player.usingItem = player.blockingSlot.has_value();
        announce(player, watchers);
    }
}

} // namespace basalt::server::runtime::bedrock::shield
