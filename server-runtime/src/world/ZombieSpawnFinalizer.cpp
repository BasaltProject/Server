#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

namespace basalt::server::runtime::world::zombieSpawnFinalizer {
namespace {

using entity::AttributeOperation;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr float kPickupChance = 0.55F;
constexpr float kBabyChance = 0.05F;
constexpr float kBreakDoorChance = 0.1F;
constexpr float kChickenJockeyChance = 0.05F;
constexpr float kNautilusShellChance = 0.03F;
constexpr float kDrownedVehicleChance = 0.5F;
constexpr float kCamelHuskChance = 0.1F;
constexpr float kReinforcementBaseRange = 0.1F;
constexpr float kKnockbackBonusRange = 0.05F;
constexpr double kFollowRangeBonusRange = 1.5;
constexpr double kFollowRangeBonusThreshold = 1.0;
constexpr float kLeaderChance = 0.05F;
constexpr double kLeaderReinforcementRange = 0.25;
constexpr double kLeaderReinforcementMin = 0.5;
constexpr double kLeaderHealthRange = 3.0;
constexpr double kLeaderHealthMin = 1.0;
constexpr double kFollowRangeBase = 35.0;
constexpr double kMaxHealthBase = 20.0;

constexpr std::string_view kReinforcements = "minecraft:spawn_reinforcements_chance";
constexpr std::string_view kKnockbackResistance = "minecraft:knockback_resistance";
constexpr std::string_view kFollowRange = "minecraft:follow_range";
constexpr std::string_view kMaxHealth = "minecraft:max_health";
constexpr std::string_view kRandomSpawnBonus = "minecraft:random_spawn_bonus";
constexpr std::string_view kZombieRandomSpawnBonus = "minecraft:zombie_random_spawn_bonus";
constexpr std::string_view kLeaderZombieBonus = "minecraft:leader_zombie_bonus";

[[nodiscard]] bool isZombieType(std::string_view type) {
    static constexpr std::array<std::string_view, 5> kTypes{"zombie", "zombie_villager", "husk", "drowned",
                                                            "zombified_piglin"};
    return std::ranges::find(kTypes, type) != kTypes.end();
}

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] bool booleanAt(const NbtCompound& data, std::string_view key) {
    const NbtTag* found = data.find(key);
    const auto* byte = found == nullptr ? nullptr : found->get<std::int8_t>();
    return byte != nullptr && *byte != 0;
}

/// One recursive finalization of something this zombie created, always as its own group.
void finalizeChild(MobSpawnForestBuilder& builder, MobSpawnNode& child, const MobSpawnContext& context) {
    MobSpawnGroupData group;
    random::RandomSource entityRandom = context.entityRandom();
    mobSpawnFinalization::writeGeneratedNode(builder, child, 0, context, group, blockOf(child.x()), blockOf(child.y()),
                                             blockOf(child.z()), entityRandom);
}

void maybeSpawnChickenJockey(MobSpawnForestBuilder& builder, MobSpawnNode& zombie, MobSpawnGroupData& group,
                             const MobSpawnContext& context) {
    if (!group.zombieCanSpawnJockey) {
        return;
    }
    random::RandomSource& random = *context.random;
    if (random.nextFloat() < kChickenJockeyChance) {
        // parity: a jockey first looks for a chicken already standing there, and rides nothing at
        // all when there is none: it does not fall through to making one.
        if (builder.environment().nearbyUnriddenChicken) {
            if (const std::optional<std::int32_t> chicken = builder.environment().nearbyUnriddenChicken(zombie);
                chicken.has_value()) {
                static_cast<void>(builder.mountExternal(zombie, *chicken));
            }
        }
        return;
    }
    if (random.nextFloat() >= kChickenJockeyChance) {
        return;
    }
    MobSpawnNode& chicken =
        builder.create(Identifier::vanilla("chicken"), zombie.x(), zombie.y(), zombie.z(), zombie.yaw());
    MobSpawnContext jockeyContext = context;
    jockeyContext.reason = MobSpawnReason::Jockey;
    finalizeChild(builder, chicken, jockeyContext);
    chicken.data().putBoolean("IsChickenJockey", true);
    static_cast<void>(builder.mount(zombie, chicken));
}

void finalizeDrowned(MobSpawnForestBuilder& builder, MobSpawnNode& drowned, const MobSpawnContext& context) {
    random::RandomSource& random = *context.random;
    const SpawnEquipmentFinalizer& equipment = context.equipmentOrFallback();
    if (!equipment.itemId(drowned.data(), "offhand").has_value() && random.nextFloat() < kNautilusShellChance) {
        equipment.setItem(drowned.data(), "offhand", "nautilus_shell");
        equipment.setGuaranteedDrop(drowned.data(), "offhand");
    }
    static const std::set<std::string, std::less<>> kFrequentDrownedBiomes{"river", "frozen_river"};
    const bool rideable = context.reason == MobSpawnReason::Natural || context.reason == MobSpawnReason::Structure;
    if (!rideable || equipment.itemId(drowned.data(), "mainhand") != "minecraft:trident" ||
        random.nextFloat() >= kDrownedVehicleChance || booleanAt(drowned.data(), "IsBaby") ||
        kFrequentDrownedBiomes.contains(context.biomePath())) {
        return;
    }
    MobSpawnNode& vehicle =
        builder.create(Identifier::vanilla("zombie_nautilus"), drowned.x(), drowned.y(), drowned.z(), drowned.yaw());
    if (context.reason == MobSpawnReason::Structure) {
        vehicle.data().putBoolean("PersistenceRequired", true);
    }
    finalizeChild(builder, vehicle, context);
    static_cast<void>(builder.mount(drowned, vehicle));
}

void finalizeHusk(MobSpawnForestBuilder& builder, MobSpawnNode& husk, const MobSpawnContext& context) {
    random::RandomSource& random = *context.random;
    const SpawnEquipmentFinalizer& equipment = context.equipmentOrFallback();
    husk.data().putBoolean("CanPickUpLoot", random.nextFloat() < kPickupChance * context.specialMultiplier);
    if (context.reason != MobSpawnReason::Natural) {
        return;
    }
    const std::int32_t blockX = blockOf(husk.x());
    const std::int32_t blockY = blockOf(husk.y());
    const std::int32_t blockZ = blockOf(husk.z());
    if (builder.environment().canSpawnEntity &&
        !builder.environment().canSpawnEntity(Identifier::vanilla("camel_husk"), blockX + 0.5,
                                              static_cast<double>(blockY), blockZ + 0.5)) {
        return;
    }
    if (random.nextFloat() >= kCamelHuskChance) {
        return;
    }
    equipment.setItem(husk.data(), "mainhand", "iron_spear");
    MobSpawnNode& camel = builder.create(Identifier::vanilla("camel_husk"), husk.x(), husk.y(), husk.z(), husk.yaw());
    finalizeChild(builder, camel, context);
    static_cast<void>(builder.mount(husk, camel));

    MobSpawnNode& parched = builder.create(Identifier::vanilla("parched"), husk.x(), husk.y(), husk.z(), husk.yaw());
    finalizeChild(builder, parched, context);
    static_cast<void>(builder.mount(parched, camel));
}

void writeAttributes(NbtCompound& data, std::string_view type, float specialMultiplier, random::RandomSource& random) {
    const double reinforcementBase =
        type == "zombified_piglin" ? 0.0 : random.nextDouble() * static_cast<double>(kReinforcementBaseRange);
    spawnAttributeNbt::setBase(data, kReinforcements, reinforcementBase);
    spawnAttributeNbt::addModifier(data, kKnockbackResistance, 0.0, kRandomSpawnBonus,
                                   random.nextDouble() * static_cast<double>(kKnockbackBonusRange),
                                   AttributeOperation::AddValue);
    const double followRangeBonus =
        random.nextDouble() * kFollowRangeBonusRange * static_cast<double>(specialMultiplier);
    if (followRangeBonus > kFollowRangeBonusThreshold) {
        spawnAttributeNbt::addModifier(data, kFollowRange, kFollowRangeBase, kZombieRandomSpawnBonus, followRangeBonus,
                                       AttributeOperation::AddMultipliedTotal);
    }
    if (random.nextFloat() >= specialMultiplier * kLeaderChance) {
        return;
    }
    const double reinforcementBonus = random.nextDouble() * kLeaderReinforcementRange + kLeaderReinforcementMin;
    const double healthBonus = random.nextDouble() * kLeaderHealthRange + kLeaderHealthMin;
    spawnAttributeNbt::addModifier(data, kReinforcements, reinforcementBase, kLeaderZombieBonus, reinforcementBonus,
                                   AttributeOperation::AddValue);
    spawnAttributeNbt::addModifier(data, kMaxHealth, kMaxHealthBase, kLeaderZombieBonus, healthBonus,
                                   AttributeOperation::AddMultipliedTotal);
    data.set("Health", NbtTag::ofFloat(static_cast<float>(kMaxHealthBase * (1.0 + healthBonus))));
    data.putBoolean("CanBreakDoors", true);
}

} // namespace

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, MobSpawnGroupData& group,
                 const MobSpawnContext& context, random::RandomSource& entityRandom) {
    const std::string& type = node.type().path();
    if (!isZombieType(type)) {
        return;
    }
    random::RandomSource& random = *context.random;
    NbtCompound& data = node.data();
    data.putBoolean("CanPickUpLoot", random.nextFloat() < kPickupChance * context.specialMultiplier);
    if (!group.zombieBaby.has_value()) {
        group.zombieBaby = random.nextFloat() < kBabyChance;
    }
    if (*group.zombieBaby) {
        data.putBoolean("IsBaby", true);
        maybeSpawnChickenJockey(builder, node, group, context);
    }
    data.putBoolean("CanBreakDoors", random.nextFloat() < context.specialMultiplier * kBreakDoorChance);
    context.equipmentOrFallback().finalizeZombieEquipmentAfterCommon(data, type, random, context.specialMultiplier,
                                                                     context.difficulty);
    writeAttributes(data, type, context.specialMultiplier, entityRandom);

    if (type == "drowned") {
        finalizeDrowned(builder, node, context);
    } else if (type == "husk") {
        finalizeHusk(builder, node, context);
    }
}

} // namespace basalt::server::runtime::world::zombieSpawnFinalizer
