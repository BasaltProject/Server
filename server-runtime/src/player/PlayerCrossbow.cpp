#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/item/ItemDamageEffects.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerCrossbow.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace basalt::server::runtime::player::crossbow {
namespace {

using storage::StoredItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
namespace mth = wire::common::mth;

constexpr std::string_view kCharged = "minecraft:charged_projectiles";
constexpr std::string_view kIntangible = "minecraft:intangible_projectile";
constexpr std::string_view kRocket = "minecraft:firework_rocket";
/// parity: `CrossbowItem.MAX_CHARGE_DURATION`, `ARROW_POWER` and `FIREWORK_POWER`.
constexpr float kChargeSeconds = 1.25F;
constexpr float kArrowPower = 3.15F;
constexpr float kRocketPower = 1.6F;
constexpr float kStartSound = 0.2F;
constexpr float kMidSound = 0.5F;
/// parity: `CrossbowItem.createProjectile`'s rocket and `AbstractArrow`'s arrow.
constexpr double kEyeHeight = 1.62;
constexpr double kRocketDrop = 0.15;
constexpr double kArrowDrop = 0.1;

[[nodiscard]] bool isArrow(std::string_view id) {
    return id == "minecraft:arrow" || id == "minecraft:spectral_arrow" || id == "minecraft:tipped_arrow";
}

[[nodiscard]] float levelValue(const NbtTag* value, std::int32_t level) {
    if (value == nullptr) {
        return 0.0F;
    }
    if (const auto* number = value->get<float>(); number != nullptr) {
        return *number;
    }
    if (const auto* number = value->get<double>(); number != nullptr) {
        return static_cast<float>(*number);
    }
    if (const auto* number = value->get<std::int32_t>(); number != nullptr) {
        return static_cast<float>(*number);
    }
    const auto* spec = value->get<NbtCompound>();
    if (spec == nullptr) {
        return 0.0F;
    }
    if (spec->getString("type") == "minecraft:linear") {
        return spec->getFloat("base") + (spec->getFloat("per_level_above_first") * static_cast<float>(level - 1));
    }
    return spec->getFloat("value");
}

/// parity: `EnchantmentHelper`'s value effects over each enchantment for the `add` kind.
[[nodiscard]] float enchantedValue(const BreakContext& context, const StoredItemStack& weapon, std::string_view key,
                                   float base) {
    float value = base;
    for (const auto& [id, level] : enchantmentLevels(weapon)) {
        const NbtCompound* definition =
            context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        const NbtCompound* effects = definition == nullptr ? nullptr : definition->getCompound("effects");
        const NbtTag* effect = effects == nullptr ? nullptr : effects->find(key);
        if (effect == nullptr) {
            continue;
        }
        std::vector<const NbtCompound*> entries;
        if (const auto* single = effect->get<NbtCompound>(); single != nullptr) {
            entries.push_back(single);
        } else if (const auto* list = effect->get<NbtList>(); list != nullptr) {
            for (const NbtTag& each : *list) {
                if (const auto* conditional = each.get<NbtCompound>(); conditional != nullptr) {
                    const NbtCompound* inner = conditional->getCompound("effect");
                    entries.push_back(inner == nullptr ? conditional : inner);
                }
            }
        }
        for (const NbtCompound* entry : entries) {
            if (entry->getString("type") == "minecraft:add") {
                value += levelValue(entry->find("value"), level);
            }
        }
    }
    return value;
}

/// parity: `CrossbowItem.getChargeDuration`.
[[nodiscard]] std::int32_t chargeDuration(const BreakContext& context, const StoredItemStack& weapon) {
    return mth::floor(
        static_cast<double>(enchantedValue(context, weapon, "minecraft:crossbow_charge_time", kChargeSeconds) * 20.0F));
}

/// parity: `CrossbowItem.getChargingSounds`.
[[nodiscard]] std::optional<std::string> chargingSound(const BreakContext& context, const StoredItemStack& weapon,
                                                       std::string_view which) {
    for (const auto& [id, level] : enchantmentLevels(weapon)) {
        const NbtCompound* definition =
            context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        const NbtCompound* effects = definition == nullptr ? nullptr : definition->getCompound("effects");
        const NbtList* sounds = effects == nullptr ? nullptr : effects->getList("minecraft:crossbow_charging_sounds");
        if (sounds == nullptr || sounds->size() == 0) {
            continue;
        }
        const std::size_t index = std::min(static_cast<std::size_t>(std::max(level, 1)), sounds->size()) - 1;
        const auto* entry = (sounds->begin() + static_cast<std::ptrdiff_t>(index))->get<NbtCompound>();
        if (entry == nullptr || !entry->contains(which)) {
            return std::nullopt;
        }
        std::string name{entry->getString(which)};
        return name.starts_with("minecraft:") ? name.substr(10) : name;
    }
    if (which == "start") {
        return "item.crossbow.loading_start";
    }
    return which == "mid" ? std::optional<std::string>{"item.crossbow.loading_middle"}
                          : std::optional<std::string>{"item.crossbow.loading_end"};
}

void playNear(const BreakContext& context, const PlayerSession& player, world::LoadedWorld& world,
              std::string_view sound, float volume, float pitch) {
    const std::int64_t seed = context.nextSeed ? context.nextSeed() : 0;
    for (const PlayerSession* heard : playersIn(context, world.name())) {
        playSoundFor(*heard, sound, wire::protocol::play::SoundSource::Players, player.x, player.y, player.z, volume,
                     pitch, seed);
    }
}

void changed(const BreakContext& context, PlayerSession& player, std::int32_t slot) {
    if (context.slotChanged) {
        context.slotChanged(player, slot);
    }
}

/// parity: `ProjectileWeaponItem.draw`.
[[nodiscard]] std::vector<StoredItemStack> draw(const BreakContext& context, PlayerSession& player,
                                                const StoredItemStack& weapon) {
    const std::optional<std::int32_t> slot = ammoSlot(player);
    const bool creative = player.gamemode == wire::protocol::play::Gamemode::Creative;
    StoredItemStack ammo = slot.has_value() && player.inventory.at(*slot).has_value()
                               ? *player.inventory.at(*slot)
                               : StoredItemStack{.id = "minecraft:arrow", .count = 1};
    if (!slot.has_value() && !creative) {
        return {};
    }
    const auto count = static_cast<std::int32_t>(enchantedValue(context, weapon, "minecraft:projectile_count", 1.0F));
    std::vector<StoredItemStack> drawn;
    for (std::int32_t index = 0; index < count; ++index) {
        StoredItemStack one = ammo;
        one.count = 1;
        if (index > 0 || creative) {
            one.components.set(kIntangible, NbtTag::ofCompound(NbtCompound{}));
        } else if (slot.has_value()) {
            std::optional<StoredItemStack> left = player.inventory.at(*slot);
            left->count -= 1;
            player.inventory.set(*slot, left->count <= 0 ? std::nullopt : left);
            changed(context, player, *slot);
        }
        drawn.push_back(std::move(one));
    }
    return drawn;
}

[[nodiscard]] std::array<float, 3> viewVector(float pitch, float yaw) {
    const float xRot = pitch * (std::numbers::pi_v<float> / 180.0F);
    const float yRot = -yaw * (std::numbers::pi_v<float> / 180.0F);
    const float cosY = mth::cos(yRot);
    const float sinY = mth::sin(yRot);
    const float cosX = mth::cos(xRot);
    const float sinX = mth::sin(xRot);
    return {sinY * cosX, -sinX, cosY * cosX};
}

/// parity: `shootProjectile`'s view vector turned about the shooter's own up vector.
[[nodiscard]] std::array<float, 3> shotVector(const PlayerSession& player, float angleDegrees) {
    const std::array<float, 3> view = viewVector(player.pitch, player.yaw);
    const std::array<float, 3> up = viewVector(player.pitch - 90.0F, player.yaw);
    const float angle = angleDegrees * (std::numbers::pi_v<float> / 180.0F);
    const float length = std::sqrt((up[0] * up[0]) + (up[1] * up[1]) + (up[2] * up[2]));
    const float ax = up[0] / length;
    const float ay = up[1] / length;
    const float az = up[2] / length;
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);
    const float dot = (ax * view[0]) + (ay * view[1]) + (az * view[2]);
    const std::array<float, 3> cross{(ay * view[2]) - (az * view[1]), (az * view[0]) - (ax * view[2]),
                                     (ax * view[1]) - (ay * view[0])};
    return {(view[0] * cosA) + (cross[0] * sinA) + (ax * dot * (1.0F - cosA)),
            (view[1] * cosA) + (cross[1] * sinA) + (ay * dot * (1.0F - cosA)),
            (view[2] * cosA) + (cross[2] * sinA) + (az * dot * (1.0F - cosA))};
}

/// parity: `getShotPitch`, level for the first and a random one high or low for the rest.
[[nodiscard]] float shotPitch(random::RandomSource& random, std::int32_t index) {
    if (index == 0) {
        return 1.0F;
    }
    const float decider = (index & 1) == 1 ? 0.63F : 0.43F;
    return (1.0F / ((random.nextFloat() * 0.5F) + 1.8F)) + decider;
}

/// parity: `ItemStack.hurtAndBreak`: unbreaking may spare it, the last point breaks it.
[[nodiscard]] bool wear(const BreakContext& context, PlayerSession& player, std::int32_t slot, std::int32_t amount,
                        random::RandomSource& random) {
    if (player.gamemode == wire::protocol::play::Gamemode::Creative) {
        return true;
    }
    const std::optional<StoredItemStack> weapon = player.inventory.at(slot);
    if (!weapon.has_value()) {
        return false;
    }
    const item::ItemDamageEffects effects{
        [&context](const wire::common::Identifier& id) {
            return context.enchantments == nullptr ? nullptr : context.enchantments->definitionOf(id);
        },
        [&context](const std::string_view tag, const wire::common::Identifier& item) {
            return context.itemTags != nullptr && world::realItemTags::contains(*context.itemTags, tag, item);
        }};
    const std::optional<StoredItemStack> worn =
        item::itemWear::hurt(*weapon, amount, random,
                             [&effects](const StoredItemStack& stack, std::int32_t cost, random::RandomSource& source) {
                                 return effects.process(stack, cost, source);
                             });
    world::triggerFacts::durabilityChanged(context.triggers, player, *weapon, worn);
    player.inventory.set(slot, worn);
    changed(context, player, slot);
    return worn.has_value();
}

} // namespace

bool isCrossbow(const StoredItemStack& stack) { return stack.id == "minecraft:crossbow"; }

bool isCharged(const StoredItemStack& stack) {
    const NbtList* charged = stack.components.getList(kCharged);
    return charged != nullptr && charged->size() > 0;
}

std::optional<std::int32_t> ammoSlot(const PlayerSession& player) {
    const auto heldAmmo = [&player](std::int32_t slot) {
        const std::optional<StoredItemStack>& stack = player.inventory.at(slot);
        return stack.has_value() && (isArrow(stack->id) || stack->id == kRocket);
    };
    if (heldAmmo(PlayerInventory::kOffhandSlot)) {
        return PlayerInventory::kOffhandSlot;
    }
    const std::int32_t mainHand = PlayerInventory::kHotbarStart + player.inventory.heldSlot();
    if (heldAmmo(mainHand)) {
        return mainHand;
    }
    return player.inventory.arrowSlot();
}

void tickCharging(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                  std::int32_t timeHeld, Charging& charging) {
    std::optional<StoredItemStack> weapon = player.inventory.at(slot);
    if (!weapon.has_value() || !isCrossbow(*weapon)) {
        return;
    }
    const std::int32_t duration = chargeDuration(context, *weapon);
    const float progress = duration <= 0 ? 1.0F : static_cast<float>(timeHeld) / static_cast<float>(duration);
    if (progress < kStartSound) {
        charging = {};
    }
    if (progress >= kStartSound && !charging.startPlayed) {
        charging.startPlayed = true;
        if (const std::optional<std::string> sound = chargingSound(context, *weapon, "start"); sound.has_value()) {
            playNear(context, player, world, *sound, 0.5F, 1.0F);
        }
    }
    if (progress >= kMidSound && !charging.midPlayed) {
        charging.midPlayed = true;
        if (const std::optional<std::string> sound = chargingSound(context, *weapon, "mid"); sound.has_value()) {
            playNear(context, player, world, *sound, 0.5F, 1.0F);
        }
    }
    if (progress < 1.0F || isCharged(*weapon)) {
        return;
    }
    const std::vector<StoredItemStack> drawn = draw(context, player, *weapon);
    if (drawn.empty()) {
        return;
    }
    std::vector<NbtTag> items;
    for (const StoredItemStack& each : drawn) {
        NbtCompound item;
        item.putString("id", each.id);
        item.putInt("count", each.count);
        if (each.components.size() > 0) {
            item.set("components", NbtTag::ofCompound(each.components));
        }
        items.push_back(NbtTag::ofCompound(std::move(item)));
    }
    weapon = player.inventory.at(slot);
    weapon->components.set(kCharged, NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, std::move(items)}));
    player.inventory.set(slot, weapon);
    changed(context, player, slot);
    if (const std::optional<std::string> sound = chargingSound(context, *weapon, "end"); sound.has_value()) {
        const double roll = context.nextDouble ? context.nextDouble() : 0.5;
        playNear(context, player, world, *sound, 1.0F, (1.0F / ((static_cast<float>(roll) * 0.5F) + 1.0F)) + 0.2F);
    }
}

void shoot(const BreakContext& context, FireworkSystem* fireworks, PlayerSession& player, world::LoadedWorld& world,
           std::int32_t slot) {
    std::optional<StoredItemStack> weapon = player.inventory.at(slot);
    if (!weapon.has_value() || !isCharged(*weapon)) {
        return;
    }
    std::vector<StoredItemStack> projectiles;
    for (const NbtTag& each : *weapon->components.getList(kCharged)) {
        const auto* item = each.get<NbtCompound>();
        if (item == nullptr) {
            continue;
        }
        const NbtCompound* components = item->getCompound("components");
        projectiles.push_back(StoredItemStack{.id = std::string{item->getString("id")},
                                              .count = item->getInt("count", 1),
                                              .components = components == nullptr ? NbtCompound{} : *components});
    }
    static_cast<void>(weapon->components.remove(kCharged));
    player.inventory.set(slot, weapon);
    changed(context, player, slot);
    const bool rockets =
        std::ranges::any_of(projectiles, [](const StoredItemStack& each) { return each.id == kRocket; });
    const float power = rockets ? kRocketPower : kArrowPower;
    // parity: `ProjectileWeaponItem.shoot`: the spread fanned out either side of the aim.
    const float spread = enchantedValue(context, *weapon, "minecraft:projectile_spread", 0.0F);
    const auto size = static_cast<std::int32_t>(projectiles.size());
    const float step = size == 1 ? 0.0F : 2.0F * spread / static_cast<float>(size - 1);
    const float offset = static_cast<float>((size - 1) % 2) * step / 2.0F;
    float direction = 1.0F;
    random::RandomSource random = random::RandomSource::legacy(context.nextSeed ? context.nextSeed() : 0);
    const auto pierce =
        static_cast<std::int8_t>(enchantedValue(context, *weapon, "minecraft:projectile_piercing", 0.0F));
    for (std::int32_t index = 0; index < size; ++index) {
        const StoredItemStack& projectile = projectiles[static_cast<std::size_t>(index)];
        const float angle = offset + (direction * static_cast<float>((index + 1) / 2) * step);
        direction = -direction;
        const std::array<float, 3> aim = shotVector(player, angle);
        if (projectile.id == kRocket) {
            if (fireworks != nullptr) {
                NbtCompound item;
                item.putString("id", projectile.id);
                item.putInt("count", 1);
                if (projectile.components.size() > 0) {
                    item.set("components", NbtTag::ofCompound(projectile.components));
                }
                static_cast<void>(fireworks->shoot(world, std::move(item), player.x,
                                                   player.y + kEyeHeight - kRocketDrop, player.z,
                                                   static_cast<double>(aim[0]), static_cast<double>(aim[1]),
                                                   static_cast<double>(aim[2]), power, 1.0F, player.entityId()));
            }
        } else {
            const bool spectral = projectile.id == "minecraft:spectral_arrow";
            const entity::projectiles::Kind& kind =
                spectral ? *entity::projectiles::kindOf(wire::common::Identifier::vanilla("spectral_arrow"))
                         : entity::projectiles::arrow();
            const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(kind.type);
            if (entry != nullptr && context.registry != nullptr) {
                entity::ServerEntity& arrow = entity::projectiles::spawn(
                    world.state(), kind, context.registry->allocateEntityId(), entry->protocolId, player.x,
                    player.y + kEyeHeight - kArrowDrop, player.z, static_cast<double>(aim[0]),
                    static_cast<double>(aim[1]), static_cast<double>(aim[2]), power, 1.0F, random);
                arrow.ownerId = player.entityId();
                arrow.critical = true;
                arrow.pierceLevel = pierce;
                arrow.arrowPickup = projectile.components.contains(kIntangible)
                                        ? entity::ServerEntity::ArrowPickup::CreativeOnly
                                        : entity::ServerEntity::ArrowPickup::Allowed;
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                    wire::common::Identifier::parse(projectile.id);
                arrow.heldItem = std::string{id.has_value() ? id->path() : "arrow"};
                arrow.heldItemComponents = projectile.components;
                arrow.firedFromWeapon = wire::common::Identifier::vanilla("crossbow");
                arrow.firedFromWeaponComponents = weapon->components;
            }
        }
        playNear(context, player, world, "item.crossbow.shoot", 1.0F, shotPitch(random, index));
        if (!wear(context, player, slot, projectile.id == kRocket ? 3 : 1, random)) {
            break;
        }
    }
    // parity: `CrossbowItem.performShooting` ends in `SHOT_CROSSBOW` and the used statistic.
    if (context.triggers != nullptr) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(weapon->id);
        if (id.has_value()) {
            context.triggers->fire(
                "shot_crossbow", player.name(), player.uuid().toString(),
                world::TriggerEvent{
                    .items = {{"item", world::TriggerItem{.item = *id, .components = weapon->components}}}});
        }
    }
    statAwards::used(player, weapon->id);
}

} // namespace basalt::server::runtime::player::crossbow
