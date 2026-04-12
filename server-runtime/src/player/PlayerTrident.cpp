#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/item/ItemComponents.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerTrident.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/WeaponDamage.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::player::trident {
namespace {

using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SoundSource;
namespace mth = wire::common::mth;

/// parity: `AbstractArrow(owner)`, the eyes less a tenth.
constexpr double kLaunchBelowEyes = static_cast<double>(0.1F);
constexpr std::string_view kThrowSound = "minecraft:item.trident.throw";

[[nodiscard]] world::weaponDamage::DefinitionOf definitionsOf(const BreakContext& context) {
    return [&context](const Identifier& id) {
        return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
    };
}

/// parity: `ItemStack.nextDamageWillBreak`.
[[nodiscard]] bool nextUseBreaks(const storage::StoredItemStack& stack) {
    return item::itemWear::isDamageable(stack) && item::itemWear::damage(stack) >= item::itemWear::maxDamage(stack) - 1;
}

void playAround(const BreakContext& context, const world::LoadedWorld& world, std::string_view sound, double x,
                double y, double z) {
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, sound, SoundSource::Players, x, y, z, 1.0F, 1.0F, seed);
    }
}

} // namespace

bool isTrident(const storage::StoredItemStack& stack) { return stack.id == "minecraft:trident"; }

bool inWaterOrRain(world::LoadedWorld& world, const PlayerSession& player) {
    const std::optional<std::int32_t> feet =
        world.state().blockState(mth::floor(player.x), mth::floor(player.y), mth::floor(player.z));
    const bool water = feet.has_value() && world::fluids::of(*feet) == world::Submersion::Water;
    return water || world::precipitation::entityInRain(world, player.x, player.y, player.z, 1.8);
}

float riptideStrength(const BreakContext& context, const storage::StoredItemStack& stack) {
    return world::weaponDamage::valueEffect(stack, "minecraft:trident_spin_attack_strength", 0.0F,
                                            definitionsOf(context));
}

bool canRaise(const BreakContext& context, world::LoadedWorld& world, const PlayerSession& player,
              const storage::StoredItemStack& stack) {
    if (nextUseBreaks(stack)) {
        return false;
    }
    return riptideStrength(context, stack) <= 0.0F || inWaterOrRain(world, player);
}

void release(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
             std::int32_t heldTicks) {
    const std::optional<storage::StoredItemStack> held = player.inventory.at(slot);
    if (!held.has_value() || !isTrident(*held) || heldTicks < kThrowThresholdTicks) {
        return;
    }
    const float riptide = riptideStrength(context, *held);
    if ((riptide > 0.0F && (!inWaterOrRain(world, player) || player.vehicleId.has_value())) || nextUseBreaks(*held)) {
        return;
    }
    const world::weaponDamage::DefinitionOf definitionOf = definitionsOf(context);
    const std::string sound = world::weaponDamage::highestLevelEntry(*held, "minecraft:trident_sound", definitionOf)
                                  .value_or(std::string{kThrowSound});
    statAwards::used(player, held->id);
    const bool creative = player.gamemode == Gamemode::Creative;
    // parity: `hurtWithoutBreaking(1)`, never past one short of breaking, nothing for a creative hand.
    storage::StoredItemStack worn = *held;
    if (!creative) {
        random::RandomSource wear = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
        const item::ItemDamageEffects effects{definitionOf, [&context](std::string_view tag, const Identifier& item) {
                                                  return context.itemTags != nullptr &&
                                                         world::realItemTags::contains(*context.itemTags, tag, item);
                                              }};
        const std::int32_t amount = effects.process(*held, 1, wear);
        if (amount != 0) {
            const std::int32_t damage =
                std::min(item::itemWear::damage(*held) + amount, item::itemWear::maxDamage(*held) - 1);
            worn = item::itemWear::withDamage(*held, damage);
            world::triggerFacts::durabilityChanged(context.triggers, player, *held, worn);
            player.inventory.set(slot, worn);
            if (context.slotChanged) {
                context.slotChanged(player, slot);
            }
        }
    }
    if (riptide > 0.0F) {
        // parity: `startAutoSpinAttack(20, 8.0F, stack)`; the push and the lift are the client's own.
        player.autoSpinTicks = kSpinTicks;
        player.autoSpinDamage = kBaseDamage;
        player.autoSpinItem = worn;
        player.shoulderDropPending = true;
        playAround(context, world, sound, player.x, player.y, player.z);
        return;
    }
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(Identifier::vanilla("trident"));
    const entity::projectiles::Kind* kind = entity::projectiles::kindOf(Identifier::vanilla("trident"));
    if (context.registry == nullptr || entry == nullptr || kind == nullptr) {
        return;
    }
    // parity: `consumeAndReturn(1)`, which a creative hand keeps.
    if (!creative) {
        player.inventory.set(slot, worn.count <= 1
                                       ? std::nullopt
                                       : std::optional{storage::StoredItemStack{
                                             .id = worn.id, .count = worn.count - 1, .components = worn.components}});
        if (context.slotChanged) {
            context.slotChanged(player, slot);
        }
    }
    // parity: `Projectile.shootFromRotation` with no roll, from the thrower's eyes.
    constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
    const float yRot = player.yaw * kRadians;
    const float xRot = player.pitch * kRadians;
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    entity::ServerEntity& trident = entity::projectiles::spawn(
        world.state(), *kind, context.registry->allocateEntityId(), entry->protocolId, player.x,
        player.y + world::fluids::eyeHeight(player.sneaking) - kLaunchBelowEyes, player.z,
        static_cast<double>(-mth::sin(yRot) * mth::cos(xRot)), static_cast<double>(-mth::sin(xRot)),
        static_cast<double>(mth::cos(yRot) * mth::cos(xRot)), kShootPower, 1.0F, random);
    trident.ownerId = player.entityId();
    trident.arrowPickup =
        creative ? entity::ServerEntity::ArrowPickup::CreativeOnly : entity::ServerEntity::ArrowPickup::Allowed;
    trident.heldItem = "trident";
    trident.heldItemComponents = worn.components;
    trident.firedFromWeapon = Identifier::vanilla("trident");
    trident.firedFromWeaponComponents = worn.components;
    trident.tridentLoyalty = static_cast<std::int8_t>(
        std::clamp(world::weaponDamage::valueEffect(worn, "minecraft:trident_return_acceleration", 0.0F, definitionOf),
                   0.0F, 127.0F));
    // parity: `ItemStack.hasFoil`, the glint override or any enchantment.
    const wire::nbt::NbtTag* glint = worn.components.find("minecraft:enchantment_glint_override");
    trident.tridentFoil = glint != nullptr && glint->get<std::int8_t>() != nullptr
                              ? *glint->get<std::int8_t>() != 0
                              : !player::enchantmentLevels(worn).empty();
    playAround(context, world, sound, trident.x, trident.y, trident.z);
}

} // namespace basalt::server::runtime::player::trident
