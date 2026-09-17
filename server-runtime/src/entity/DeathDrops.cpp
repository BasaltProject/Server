#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace basalt::server::runtime::entity::deathDrops {
namespace {

using namespace std::string_view_literals;

/// Every type whose class extends `Monster`.
constexpr std::array kMonsters{
    "blaze"sv,    "bogged"sv,         "breeze"sv,          "cave_spider"sv,     "creaking"sv, "creeper"sv,
    "drowned"sv,  "elder_guardian"sv, "enderman"sv,        "endermite"sv,       "evoker"sv,   "giant"sv,
    "guardian"sv, "husk"sv,           "illusioner"sv,      "parched"sv,         "piglin"sv,   "piglin_brute"sv,
    "pillager"sv, "ravager"sv,        "silverfish"sv,      "skeleton"sv,        "spider"sv,   "stray"sv,
    "vex"sv,      "vindicator"sv,     "warden"sv,          "witch"sv,           "wither"sv,   "wither_skeleton"sv,
    "zoglin"sv,   "zombie"sv,         "zombie_villager"sv, "zombified_piglin"sv};

/// Every type whose class extends `Animal`.
constexpr std::array kAnimals{"armadillo"sv, "axolotl"sv,        "bee"sv,          "camel"sv,          "camel_husk"sv,
                              "cat"sv,       "chicken"sv,        "cow"sv,          "donkey"sv,         "fox"sv,
                              "frog"sv,      "goat"sv,           "happy_ghast"sv,  "hoglin"sv,         "horse"sv,
                              "llama"sv,     "mooshroom"sv,      "mule"sv,         "nautilus"sv,       "ocelot"sv,
                              "panda"sv,     "parrot"sv,         "pig"sv,          "polar_bear"sv,     "rabbit"sv,
                              "sheep"sv,     "skeleton_horse"sv, "sniffer"sv,      "strider"sv,        "trader_llama"sv,
                              "turtle"sv,    "wolf"sv,           "zombie_horse"sv, "zombie_nautilus"sv};

/// Every type whose class extends `WaterAnimal` or `AgeableWaterCreature`.
constexpr std::array kWaterCreatures{"cod"sv,    "dolphin"sv, "glow_squid"sv, "pufferfish"sv,
                                     "salmon"sv, "squid"sv,   "tadpole"sv,    "tropical_fish"sv};

/// parity: the zombie family, where a baby is worth two and a half times as much.
constexpr std::array kZombies{"drowned"sv, "husk"sv, "zombie"sv, "zombie_villager"sv, "zombified_piglin"sv};

/// parity: `EquipmentSlot.VALUES` up to the saddle, which `canIncreaseExperience` leaves out.
constexpr std::array kExperienceSlots{"mainhand"sv, "offhand"sv, "feet"sv, "legs"sv, "chest"sv, "head"sv, "body"sv};

/// parity: `DropChances.DEFAULT_EQUIPMENT_DROP_CHANCE` and `PRESERVE_ITEM_DROP_CHANCE_THRESHOLD`.
constexpr float kDefaultDropChance = 0.085F;
constexpr float kPreserveThreshold = 1.0F;

constexpr double kBabyZombieFactor = 2.5;

[[nodiscard]] bool in(std::span<const std::string_view> names, std::string_view path) {
    return std::ranges::find(names, path) != names.end();
}

[[nodiscard]] bool isBaby(const ServerEntity& entity) { return entity.age < 0; }

/// parity: the `xpReward` each constructor sets.
[[nodiscard]] std::int32_t xpReward(const ServerEntity& entity) {
    const std::string_view path = entity.type().path();
    if (path == "blaze" || path == "guardian" || path == "elder_guardian" || path == "evoker" || path == "breeze") {
        return 10;
    }
    if (path == "ravager" || path == "piglin_brute") {
        return 20;
    }
    if (path == "wither") {
        return 50;
    }
    if (path == "vex" || path == "endermite") {
        return 3;
    }
    if (path == "creaking") {
        return 0;
    }
    if (path == "slime" || path == "magma_cube") {
        return entity.cubeSize;
    }
    if (in(kMonsters, path) || path == "ghast" || path == "phantom" || path == "shulker") {
        return 5;
    }
    return 0;
}

/// parity: `Mob.getBaseExperienceReward`, a little more for each item it wears and would not keep.
[[nodiscard]] std::int32_t withEquipment(const ServerEntity& entity, std::int32_t reward,
                                         random::RandomSource& random) {
    if (reward <= 0 || !entity.sourceNbt.has_value()) {
        return reward;
    }
    const wire::nbt::NbtCompound* equipment = entity.sourceNbt->getCompound("equipment");
    if (equipment == nullptr) {
        return reward;
    }
    const wire::nbt::NbtCompound* chances = entity.sourceNbt->getCompound("drop_chances");
    for (const std::string_view slot : kExperienceSlots) {
        const wire::nbt::NbtCompound* item = equipment->getCompound(slot);
        if (item == nullptr || item->getString("id").empty()) {
            continue;
        }
        const float chance = chances != nullptr ? chances->getFloat(slot, kDefaultDropChance) : kDefaultDropChance;
        if (chance <= kPreserveThreshold) {
            reward += 1 + random.nextInt(3);
        }
    }
    return reward;
}

} // namespace

bool dropsLoot(const ServerEntity& entity, bool mobDrops) {
    return mobDrops && (in(kMonsters, entity.type().path()) || !isBaby(entity));
}

bool dropsExperience(const ServerEntity& entity, bool mobDrops) {
    const std::string_view path = entity.type().path();
    const bool should = path != "tadpole" && (in(kMonsters, path) || path == "hoglin" || !isBaby(entity));
    return entity.lastHurtByPlayerTime > 0 && should && mobDrops;
}

std::int32_t experienceReward(const ServerEntity& entity, random::RandomSource& random) {
    const std::string_view path = entity.type().path();
    if (path == "hoglin") {
        return isBaby(entity) ? 3 : 5;
    }
    if (path == "piglin") {
        return xpReward(entity);
    }
    if (path == "sulfur_cube") {
        return isBaby(entity) ? 0 : 1 + random.nextInt(2);
    }
    if (path == "chicken" && entity.sourceNbt.has_value() && entity.sourceNbt->getBoolean("IsChickenJockey")) {
        return 10;
    }
    if (in(kAnimals, path) || in(kWaterCreatures, path)) {
        return 1 + random.nextInt(3);
    }
    std::int32_t reward = xpReward(entity);
    if (in(kZombies, path) && isBaby(entity)) {
        reward = static_cast<std::int32_t>(static_cast<double>(reward) * kBabyZombieFactor);
    }
    return withEquipment(entity, reward, random);
}

} // namespace basalt::server::runtime::entity::deathDrops
