#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::player {
namespace {

using wire::common::Identifier;
using wire::protocol::play::Gamemode;

constexpr std::string_view kEnderPearl = "ender_pearl";
constexpr std::string_view kSoundPearlThrow = "entity.ender_pearl.throw";

/// parity: `ThrowableProjectileItem.use`, which throws at this speed and with this spread.
constexpr float kPearlPower = 1.5F;
constexpr float kPearlUncertainty = 1.0F;

/// parity: what a player throws from, which is the eye rather than the feet.
constexpr double kThrowEyeHeight = 1.62;

constexpr float kHalfCircle = 180.0F;

constexpr std::string_view kBow = "bow";
constexpr std::string_view kArrowItem = "minecraft:arrow";
constexpr std::string_view kSoundArrowShoot = "entity.arrow.shoot";

/// parity: `BowItem`, twenty ticks to a full draw and a tenth of it before anything flies.
constexpr float kBowDrawTicks = 20.0F;
constexpr float kMinBowPower = 0.1F;
constexpr float kBowPowerScale = 3.0F;
constexpr float kBowUncertainty = 1.0F;

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

/// The way an aim points, as the projectiles want it.
struct Aim {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] Aim aimOf(const PlayerSession& player) {
    const float pitch = player.pitch * std::numbers::pi_v<float> / kHalfCircle;
    const float yaw = player.yaw * std::numbers::pi_v<float> / kHalfCircle;
    return Aim{.x = static_cast<double>(-std::sin(yaw) * std::cos(pitch)),
               .y = static_cast<double>(-std::sin(pitch)),
               .z = static_cast<double>(std::cos(yaw) * std::cos(pitch))};
}

/// One off the stack in that slot, and the slot said out loud.
void spendOne(const BreakContext& context, PlayerSession& player, const std::int32_t slot) {
    const std::optional<storage::StoredItemStack>& stack = player.inventory.at(slot);
    if (!stack.has_value()) {
        return;
    }
    if (stack->count <= 1) {
        player.inventory.set(slot, std::nullopt);
    } else {
        storage::StoredItemStack left = *stack;
        left.count = stack->count - 1;
        player.inventory.set(slot, left);
    }
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

} // namespace

bool throwItemFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const Identifier& item,
                  const std::int32_t slot) {
    if (item.path() != kEnderPearl) {
        return false;
    }
    const entity::projectiles::Kind* kind = entity::projectiles::kindOf(Identifier::vanilla(std::string{kEnderPearl}));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
    if (context.registry == nullptr || entry == nullptr) {
        return true;
    }
    const Aim aim = aimOf(player);
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    entity::ServerEntity& pearl = entity::projectiles::spawn(
        world.state(), *kind, context.registry->allocateEntityId(), entry->protocolId, player.x,
        player.y + kThrowEyeHeight, player.z, aim.x, aim.y, aim.z, kPearlPower, kPearlUncertainty, random);
    pearl.ownerId = player.entityId();

    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    const auto pitchOfSound = static_cast<float>(0.4 / ((roll * 0.4) + 0.8));
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, kSoundPearlThrow, wire::protocol::play::SoundSource::Neutral, player.x, player.y, player.z,
                     0.5F, pitchOfSound, seed);
    }
    if (player.gamemode == Gamemode::Creative) {
        return true;
    }
    spendOne(context, player, slot);
    return true;
}

float bowPowerOf(const std::int32_t drawnTicks) {
    const float drawn = static_cast<float>(drawnTicks) / kBowDrawTicks;
    return std::min(((drawn * drawn) + (drawn * 2.0F)) / 3.0F, 1.0F);
}

void shootBowFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const std::int32_t slot,
                 const std::int32_t drawnTicks, const ShotGate& allowed) {
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    const std::optional<Identifier> heldId = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!heldId.has_value() || heldId->path() != kBow) {
        return;
    }
    const float power = bowPowerOf(drawnTicks);
    if (power < kMinBowPower) {
        return;
    }
    const bool creative = player.gamemode == Gamemode::Creative;
    const std::optional<std::int32_t> arrowSlot = player.inventory.arrowSlot();
    if (!creative && !arrowSlot.has_value()) {
        return;
    }
    // parity: `Player.getProjectile` hands a creative player a plain arrow when the bag has none.
    const storage::StoredItemStack ammo = arrowSlot.has_value() && player.inventory.at(*arrowSlot).has_value()
                                              ? *player.inventory.at(*arrowSlot)
                                              : storage::StoredItemStack{.id = std::string{kArrowItem}, .count = 1};
    const bool spectral = ammo.id == "minecraft:spectral_arrow";
    const entity::projectiles::Kind& kind =
        spectral ? *entity::projectiles::kindOf(Identifier::vanilla("spectral_arrow")) : entity::projectiles::arrow();
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(kind.type);
    if (context.registry == nullptr || entry == nullptr) {
        return;
    }
    if (allowed && !allowed(player, kind.type, power)) {
        return;
    }
    const Aim aim = aimOf(player);
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    entity::ServerEntity& arrow = entity::projectiles::spawn(
        world.state(), kind, context.registry->allocateEntityId(), entry->protocolId, player.x,
        player.y + kThrowEyeHeight, player.z, aim.x, aim.y, aim.z, power * kBowPowerScale, kBowUncertainty, random);
    arrow.ownerId = player.entityId();
    arrow.critical = power >= 1.0F;
    // parity: `ProjectileWeaponItem.useAmmo`: creative, or Infinity on a plain arrow, spends nothing
    // and marks the arrow `intangible_projectile`, which only a creative player may pick back up.
    const bool infinite =
        creative || (ammo.id == kArrowItem && player::enchantmentLevel(*held, Identifier::vanilla("infinity")) > 0);
    arrow.arrowPickup =
        infinite ? entity::ServerEntity::ArrowPickup::CreativeOnly : entity::ServerEntity::ArrowPickup::Allowed;
    arrow.heldItem = std::string{parsedOrNothing(ammo.id).value_or(Identifier::vanilla("arrow")).path()};
    arrow.heldItemComponents = ammo.components;

    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    const auto soundPitch = static_cast<float>((1.0 / ((roll * 0.4) + 1.2)) + (static_cast<double>(power) * 0.5));
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, kSoundArrowShoot, wire::protocol::play::SoundSource::Players, player.x, player.y, player.z,
                     1.0F, soundPitch, seed);
    }
    if (infinite || !arrowSlot.has_value()) {
        return;
    }
    spendOne(context, player, *arrowSlot);
}

} // namespace basalt::server::runtime::player
