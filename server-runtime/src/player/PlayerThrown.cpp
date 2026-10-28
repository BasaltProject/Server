#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerThrown.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::player {
namespace {

using wire::common::Identifier;
using wire::protocol::play::Gamemode;

/// parity: `ThrowableProjectileItem.use`.

/// parity: what a player throws from, which is the eye rather than the feet.
constexpr double kThrowEyeHeight = 1.62;

constexpr float kHalfCircle = 180.0F;

constexpr std::string_view kBow = "bow";
constexpr std::string_view kArrowItem = "minecraft:arrow";
constexpr std::string_view kSoundArrowShoot = "entity.arrow.shoot";

/// parity: `BowItem`.
constexpr float kBowDrawTicks = 20.0F;
constexpr float kMinBowPower = 0.1F;
constexpr float kBowPowerScale = 3.0F;

[[nodiscard]] std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const auto parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

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

struct Throwable {
    std::string_view item;
    std::string_view entity;
    std::string_view kind;
    float power;
    float pitchOffset;
    std::string_view sound;
    wire::protocol::play::SoundSource source;
    bool carries;
};

[[nodiscard]] const Throwable* throwableOf(std::string_view item) {
    using wire::protocol::play::SoundSource;
    // parity: `SnowballItem`, `EggItem`, `EnderpearlItem`, `ExperienceBottleItem`, `WindChargeItem`.
    static constexpr std::array<Throwable, 9> kThrowables{{
        {"ender_pearl", "ender_pearl", "ender_pearl", 1.5F, 0.0F, "entity.ender_pearl.throw", SoundSource::Neutral,
         false},
        {"snowball", "snowball", "snowball", 1.5F, 0.0F, "entity.snowball.throw", SoundSource::Neutral, true},
        {"egg", "egg", "egg", 1.5F, 0.0F, "entity.egg.throw", SoundSource::Players, true},
        {"blue_egg", "egg", "egg", 1.5F, 0.0F, "entity.egg.throw", SoundSource::Players, true},
        {"brown_egg", "egg", "egg", 1.5F, 0.0F, "entity.egg.throw", SoundSource::Players, true},
        {"experience_bottle", "experience_bottle", "experience_bottle", 0.7F, -20.0F, "entity.experience_bottle.throw",
         SoundSource::Neutral, true},
        {"splash_potion", "splash_potion", "splash_potion", 0.5F, -20.0F, "entity.splash_potion.throw",
         SoundSource::Players, true},
        {"lingering_potion", "lingering_potion", "lingering_potion", 0.5F, -20.0F, "entity.lingering_potion.throw",
         SoundSource::Neutral, true},
        {"wind_charge", "wind_charge", "wind_charge", 1.5F, 0.0F, "entity.wind_charge.throw", SoundSource::Neutral,
         false},
    }};
    const auto found = std::ranges::find_if(kThrowables, [&](const Throwable& one) { return one.item == item; });
    return found == kThrowables.end() ? nullptr : &*found;
}

[[nodiscard]] float uncertaintyOf(const config::GameplayRules::Items& items, std::string_view item) {
    if (item == "ender_pearl") {
        return items.projectileOffsetEnderPearl;
    }
    if (item == "snowball") {
        return items.projectileOffsetSnowball;
    }
    if (item.ends_with("egg")) {
        return items.projectileOffsetEgg;
    }
    if (item.ends_with("_potion")) {
        return items.projectileOffsetThrowablePotion;
    }
    if (item == "wind_charge") {
        return items.projectileOffsetWindCharge;
    }
    return 1.0F;
}

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

bool isThrowable(const Identifier& item) { return throwableOf(item.path()) != nullptr; }

bool throwItemFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, const Identifier& item,
                  const std::int32_t slot) {
    const Throwable* throwable = throwableOf(item.path());
    if (throwable == nullptr) {
        return false;
    }
    const entity::projectiles::Kind* kind =
        entity::projectiles::kindOf(Identifier::vanilla(std::string{throwable->kind}));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr
                        : server::registry::entityTypes::of(Identifier::vanilla(std::string{throwable->entity}));
    if (context.registry == nullptr || entry == nullptr) {
        return true;
    }
    const std::optional<storage::StoredItemStack>& held = player.inventory.at(slot);
    // parity: `Projectile.shootFromRotation`, whose pitch offset only tilts the climb.
    const float pitch = (player.pitch + throwable->pitchOffset) * std::numbers::pi_v<float> / kHalfCircle;
    const float yaw = player.yaw * std::numbers::pi_v<float> / kHalfCircle;
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    entity::ServerEntity& thrown = entity::projectiles::spawn(
        world.state(), *kind, context.registry->allocateEntityId(), entry->protocolId, player.x,
        player.y + kThrowEyeHeight, player.z, static_cast<double>(-std::sin(yaw) * std::cos(pitch)),
        static_cast<double>(-std::sin(pitch)), static_cast<double>(std::cos(yaw) * std::cos(pitch)), throwable->power,
        uncertaintyOf(world.state().gameplay().items, throwable->item), random);
    thrown.ownerId = player.entityId();
    thrown.ownerUuid = player.uuid();
    if (throwable->carries && held.has_value()) {
        thrown.heldItem = std::string{item.path()};
        thrown.heldItemComponents = held->components;
    }

    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    const auto pitchOfSound = static_cast<float>(0.4 / ((roll * 0.4) + 0.8));
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, throwable->sound, throwable->source, player.x, player.y, player.z, 0.5F, pitchOfSound,
                     seed);
    }
    statAwards::used(player, item.toString());
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
    entity::ServerEntity& arrow =
        entity::projectiles::spawn(world.state(), kind, context.registry->allocateEntityId(), entry->protocolId,
                                   player.x, player.y + kThrowEyeHeight, player.z, aim.x, aim.y, aim.z,
                                   power * kBowPowerScale, world.state().gameplay().items.projectileOffsetBow, random);
    arrow.ownerId = player.entityId();
    arrow.ownerUuid = player.uuid();
    arrow.critical = power >= 1.0F;
    // parity: `ProjectileWeaponItem.useAmmo`.
    const bool infinite =
        creative || (ammo.id == kArrowItem && player::enchantmentLevel(*held, Identifier::vanilla("infinity")) > 0);
    arrow.arrowPickup =
        infinite ? entity::ServerEntity::ArrowPickup::CreativeOnly : entity::ServerEntity::ArrowPickup::Allowed;
    arrow.heldItem = std::string{parsedOrNothing(ammo.id).value_or(Identifier::vanilla("arrow")).path()};
    arrow.heldItemComponents = ammo.components;
    arrow.firedFromWeapon = parsedOrNothing(held->id);
    arrow.firedFromWeaponComponents = held->components;

    const double roll = context.nextDouble ? context.nextDouble() : 0.5;
    const auto soundPitch = static_cast<float>((1.0 / ((roll * 0.4) + 1.2)) + (static_cast<double>(power) * 0.5));
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, kSoundArrowShoot, wire::protocol::play::SoundSource::Players, player.x, player.y, player.z,
                     1.0F, soundPitch, seed);
    }
    // parity: `BowItem.releaseUsing`, right after the sound.
    statAwards::used(player, held->id);
    if (infinite || !arrowSlot.has_value()) {
        return;
    }
    spendOne(context, player, *arrowSlot);
}

} // namespace basalt::server::runtime::player
