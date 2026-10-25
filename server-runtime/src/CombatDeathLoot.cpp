#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <numbers>

namespace basalt::server::runtime {

namespace {

using player::PlayerInventory;
using wire::common::Identifier;

/// parity: `Player.getBaseExperienceReward`, seven points a level up to a hundred.
constexpr std::int32_t kExperiencePerLevel = 7;
constexpr std::int32_t kMaxDeathExperience = 100;
/// parity: `Player.createItemStackToDrop` puts the stack a little under the eyes.
constexpr double kBelowEyes = 0.3;
constexpr double kScatterSpeed = 0.5;
constexpr double kScatterLift = 0.2;

/// parity: `Inventory.dropAll`: the hotbar and the rows, then the armor feet first, then the off hand.
[[nodiscard]] std::vector<std::int32_t> dropOrder() {
    std::vector<std::int32_t> order;
    for (std::int32_t slot = PlayerInventory::kHotbarStart; slot < PlayerInventory::kMainSlots; ++slot) {
        order.push_back(slot);
    }
    for (std::int32_t slot = 0; slot < PlayerInventory::kHotbarStart; ++slot) {
        order.push_back(slot);
    }
    for (std::int32_t slot = PlayerInventory::kArmourStart + PlayerInventory::kArmourSlots - 1;
         slot >= PlayerInventory::kArmourStart; --slot) {
        order.push_back(slot);
    }
    order.push_back(PlayerInventory::kOffhandSlot);
    return order;
}

} // namespace

player::DeathLoot CombatHandler::deathLootOf(player::PlayerSession& session) const {
    player::DeathLoot loot;
    const world::LoadedWorld* world = owner_->currentWorld();
    // parity: `ServerPlayer.die` leaves a spectator alone, and `keepInventory` the rest.
    const bool keeps = session.gamemode == wire::protocol::play::Gamemode::Spectator || world == nullptr ||
                       world->gameRules().get(world::gameRules::kKeepInventory);
    loot.keepInventory = keeps;
    loot.keepLevel = keeps;
    loot.showMessages = world == nullptr || world->gameRules().get(world::gameRules::kShowDeathMessages);
    if (keeps) {
        return loot;
    }
    for (const std::int32_t slot : dropOrder()) {
        const std::optional<storage::StoredItemStack>& stack = session.inventory.at(slot);
        // parity: `destroyVanishingCursedItems`.
        if (!stack.has_value() || stack->count <= 0 ||
            player::enchantmentLevel(*stack, Identifier::vanilla("vanishing_curse")) > 0) {
            continue;
        }
        loot.drops.push_back(*stack);
    }
    loot.experience = std::min(session.experience.level() * kExperiencePerLevel, kMaxDeathExperience);
    return loot;
}

void CombatHandler::spreadDeathLoot(player::PlayerSession& session, const player::DeathLoot& loot) const {
    world::LoadedWorld* world = owner_->currentWorld();
    session.keepLevelOnRespawn = loot.keepLevel;
    if (world == nullptr) {
        return;
    }
    if (!loot.keepInventory) {
        const player::BreakContext context = owner_->blockInteraction().breakContext();
        const auto random = [&context] { return context.nextDouble ? context.nextDouble() : 0.5; };
        for (const storage::StoredItemStack& stack : loot.drops) {
            const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(stack.id);
            if (!item.has_value() || stack.count <= 0) {
                continue;
            }
            // parity: `Player.createItemStackToDrop` with random motion.
            const double power = random() * kScatterSpeed;
            const double angle = random() * 2.0 * std::numbers::pi;
            player::spawnDroppedItem(context, *world, *item, stack.count, session.x,
                                     session.y + world::fluids::eyeHeight(session.sneaking) - kBelowEyes, session.z,
                                     -std::sin(angle) * power, kScatterLift, std::cos(angle) * power,
                                     world::kDefaultPickupDelay, stack.components);
        }
        for (std::int32_t slot = 0; slot <= PlayerInventory::kOffhandSlot; ++slot) {
            session.inventory.set(slot, std::nullopt);
        }
        owner_->sendInventoryAll();
    }
    if (loot.experience > 0 && owner_->options().spawnOrb) {
        owner_->options().spawnOrb(*world, session.x, session.y, session.z, loot.experience);
    }
}

} // namespace basalt::server::runtime
