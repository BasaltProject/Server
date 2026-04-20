#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerGliding.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

namespace basalt::server::runtime::player::gliding {
namespace {

constexpr std::int32_t kChestSlot = PlayerInventory::kArmourStart + 1;
/// parity: `updateFallFlying`: a check every ten ticks and a point of wear every twenty.
constexpr std::int32_t kCheckInterval = 10;

/// parity: `LivingEntity.canGlideUsing`.
[[nodiscard]] bool gliderIn(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value()) {
        return false;
    }
    const bool glider = stack->id == "minecraft:elytra" || stack->components.contains("minecraft:glider");
    if (!glider) {
        return false;
    }
    return !item::itemWear::isDamageable(*stack) ||
           item::itemWear::damage(*stack) < item::itemWear::maxDamage(*stack) - 1;
}

[[nodiscard]] const wire::common::Identifier& levitation() {
    static const wire::common::Identifier kLevitation = wire::common::Identifier::vanilla("levitation");
    return kLevitation;
}

} // namespace

bool canGlide(const PlayerSession& player) {
    if (player.onGround || player.vehicleId.has_value() || player.effects.contains(levitation())) {
        return false;
    }
    return gliderIn(player.inventory.at(kChestSlot));
}

void startFallFlying(PlayerSession& player) {
    // parity: `isInLiquid`.
    player.fallFlying = !player.fallFlying && canGlide(player);
}

void tick(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world) {
    player.fallFlyTicks = player.fallFlying ? player.fallFlyTicks + 1 : 0;
    if (!player.fallFlying) {
        return;
    }
    if (!canGlide(player)) {
        player.fallFlying = false;
        return;
    }
    const std::int32_t next = player.fallFlyTicks + 1;
    if (next % kCheckInterval != 0) {
        return;
    }
    if ((next / kCheckInterval) % 2 == 0 && player.gamemode != wire::protocol::play::Gamemode::Creative) {
        const std::optional<storage::StoredItemStack>& wings = player.inventory.at(kChestSlot);
        random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
        const item::ItemDamageEffects effects{
            [&context](const wire::common::Identifier& id) {
                return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
            },
            [&context](const std::string_view tag, const wire::common::Identifier& item) {
                return context.itemTags != nullptr && world::realItemTags::contains(*context.itemTags, tag, item);
            }};
        const std::optional<storage::StoredItemStack> worn = item::itemWear::hurt(
            *wings, 1, random,
            [&effects](const storage::StoredItemStack& stack, std::int32_t cost, random::RandomSource& source) {
                return effects.process(stack, cost, source);
            });
        if (worn != wings) {
            world::triggerFacts::durabilityChanged(context.triggers, player, *wings, worn);
            player.inventory.set(kChestSlot, worn);
            if (context.slotChanged) {
                context.slotChanged(player, kChestSlot);
            }
        }
    }
    world::vibrations::emit(world.state(), "elytra_glide", player.x, player.y, player.z, world.gameTime(),
                            {.entity = player.uuid()});
}

} // namespace basalt::server::runtime::player::gliding
