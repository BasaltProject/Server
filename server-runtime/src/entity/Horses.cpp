#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

namespace basalt::server::runtime::entity::horses {
namespace {

using wire::nbt::NbtTag;

[[nodiscard]] bool oneOf(std::string_view path, std::initializer_list<std::string_view> names) {
    return std::ranges::find(names, path) != names.end();
}

/// parity: `#can_equip_saddle`, the horse-like part of it.
[[nodiscard]] bool takesSaddle(std::string_view path) {
    return oneOf(path, {"horse", "skeleton_horse", "zombie_horse", "donkey", "mule", "camel", "camel_husk"});
}

/// parity: `#can_wear_horse_armor`.
[[nodiscard]] bool takesHorseArmor(std::string_view path) { return oneOf(path, {"horse", "zombie_horse"}); }

/// parity: `Equippable.llamaSwag`, one per dye colour.
[[nodiscard]] bool isSwag(std::string_view path) {
    static constexpr std::array<std::string_view, 16> kColours{
        "white",      "orange", "magenta", "light_blue", "yellow", "lime",  "pink", "gray",
        "light_gray", "cyan",   "purple",  "blue",       "brown",  "green", "red",  "black"};
    return path.ends_with("_carpet") &&
           std::ranges::find(kColours, path.substr(0, path.size() - std::string_view{"_carpet"}.size())) !=
               kColours.end();
}

[[nodiscard]] bool numeric(const ServerEntity& horse, std::string_view key) {
    return horse.sourceNbt.has_value() && horse.sourceNbt->getBoolean(key);
}

} // namespace

bool isHorse(const wire::common::Identifier& type) {
    return oneOf(type.path(), {"horse", "donkey", "mule", "skeleton_horse", "zombie_horse", "llama", "trader_llama",
                               "camel", "camel_husk"});
}

bool isChested(const wire::common::Identifier& type) {
    return oneOf(type.path(), {"donkey", "mule", "llama", "trader_llama"});
}

bool isLlama(const wire::common::Identifier& type) { return oneOf(type.path(), {"llama", "trader_llama"}); }

bool isCamel(const wire::common::Identifier& type) { return oneOf(type.path(), {"camel", "camel_husk"}); }

HorseState& stateOf(ServerEntity& horse) {
    if (horse.horse == nullptr) {
        horse.horse = std::make_shared<HorseState>();
    }
    return *horse.horse;
}

std::int8_t flagsOf(const ServerEntity& horse) {
    std::int8_t flags = 0;
    if (horse.tamedHorse) {
        flags = static_cast<std::int8_t>(flags | kFlagTame);
    }
    if (numeric(horse, "Bred")) {
        flags = static_cast<std::int8_t>(flags | kFlagBred);
    }
    if (const HorseState* state = horse.horse.get(); state != nullptr) {
        if (state->eating) {
            flags = static_cast<std::int8_t>(flags | kFlagEating);
        }
        if (state->standing) {
            flags = static_cast<std::int8_t>(flags | kFlagStanding);
        }
        if (state->mouthOpen) {
            flags = static_cast<std::int8_t>(flags | kFlagOpenMouth);
        }
    }
    return flags;
}

bool tamed(const ServerEntity& horse) { return isCamel(horse.type()) || horse.tamedHorse; }

void tame(ServerEntity& horse, const wire::common::Uuid& owner) {
    horse.tamedHorse = true;
    if (!horse.sourceNbt.has_value()) {
        horse.sourceNbt.emplace();
    }
    horse.sourceNbt->set("Owner",
                         NbtTag::ofIntArray({static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.high() >> 32)),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.high())),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.low() >> 32)),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(owner.low()))}));
}

std::int32_t maxTemper(const wire::common::Identifier& type) { return isLlama(type) ? 30 : 100; }

std::int32_t temper(const ServerEntity& horse) {
    return horse.sourceNbt.has_value() ? horse.sourceNbt->getInt("Temper") : 0;
}

std::int32_t modifyTemper(ServerEntity& horse, std::int32_t amount) {
    const std::int32_t value = std::clamp(temper(horse) + amount, 0, maxTemper(horse.type()));
    if (!horse.sourceNbt.has_value()) {
        horse.sourceNbt.emplace();
    }
    horse.sourceNbt->putInt("Temper", value);
    return value;
}

bool hasChest(const ServerEntity& horse) { return isChested(horse.type()) && numeric(horse, "ChestedHorse"); }

void setChest(ServerEntity& horse, bool chest) {
    if (!horse.sourceNbt.has_value()) {
        horse.sourceNbt.emplace();
    }
    horse.sourceNbt->putBoolean("ChestedHorse", chest);
    if (!chest) {
        static_cast<void>(horse.sourceNbt->remove("Items"));
    }
}

std::int32_t inventoryColumns(const ServerEntity& horse) {
    if (!hasChest(horse)) {
        return 0;
    }
    return isLlama(horse.type()) ? strength(horse) : 5;
}

std::int32_t strength(const ServerEntity& llama) {
    // parity: `Llama.setStrength`, which clamps whatever it is given.
    return std::clamp(llama.sourceNbt.has_value() ? llama.sourceNbt->getInt("Strength") : 0, 1, 5);
}

bool saddled(const ServerEntity& horse) { return mobEquipment::get(horse, mobEquipment::Slot::Saddle).has_value(); }

bool baby(const ServerEntity& horse) { return horse.age < 0; }

bool silent(const ServerEntity& horse) { return numeric(horse, "Silent"); }

bool saddleSlotActive(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (!takesSaddle(path)) {
        return false;
    }
    // parity: `Horse`, `SkeletonHorse` and `ZombieHorse` can use every slot; the rest only grown and tame.
    if (oneOf(path, {"horse", "skeleton_horse", "zombie_horse"})) {
        return true;
    }
    return horse.health > 0.0 && !baby(horse) && tamed(horse);
}

bool bodySlotActive(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    return takesHorseArmor(path) || isLlama(horse.type());
}

Wearable wearableFor(const ServerEntity& horse, const wire::common::Identifier& item) {
    const std::string_view path = horse.type().path();
    if (item.path() == "saddle") {
        return saddleSlotActive(horse) ? Wearable::Saddle : Wearable::None;
    }
    if (item.path().ends_with("_horse_armor") && takesHorseArmor(path)) {
        return Wearable::Body;
    }
    if (isSwag(item.path()) && isLlama(horse.type())) {
        return Wearable::Body;
    }
    return Wearable::None;
}

bool isFood(const wire::common::Identifier& type, const wire::common::Identifier& item) {
    const std::string_view path = item.path();
    if (isLlama(type)) {
        return oneOf(path, {"wheat", "hay_block"});
    }
    if (isCamel(type)) {
        return path == "cactus";
    }
    if (type.path() == "zombie_horse") {
        return path == "red_mushroom";
    }
    return oneOf(path, {"wheat", "sugar", "hay_block", "apple", "carrot", "golden_carrot", "golden_apple",
                        "enchanted_golden_apple"});
}

Feeding feedingOf(const wire::common::Identifier& type, const wire::common::Identifier& item) {
    const std::string_view path = item.path();
    if (isLlama(type)) {
        if (path == "wheat") {
            return {.heal = 2.0F, .ageUp = 10, .temper = 3};
        }
        if (path == "hay_block") {
            return {.heal = 10.0F, .ageUp = 90, .temper = 6, .love = true};
        }
        return {};
    }
    if (path == "wheat") {
        return {.heal = 2.0F, .ageUp = 20, .temper = 3};
    }
    if (path == "sugar") {
        return {.heal = 1.0F, .ageUp = 30, .temper = 3};
    }
    if (path == "hay_block") {
        return {.heal = 20.0F, .ageUp = 180};
    }
    if (path == "apple" || path == "carrot") {
        return {.heal = 3.0F, .ageUp = 60, .temper = 3};
    }
    if (path == "red_mushroom") {
        return {.heal = 3.0F, .temper = 3};
    }
    if (path == "golden_carrot") {
        return {.heal = 4.0F, .ageUp = 60, .temper = 5, .love = true};
    }
    if (path == "golden_apple" || path == "enchanted_golden_apple") {
        return {.heal = 10.0F, .ageUp = 240, .temper = 10, .love = true};
    }
    return {};
}

bool canRear(const wire::common::Identifier& type) { return !isCamel(type) && !isLlama(type); }

bool canEatGrass(const wire::common::Identifier& type) { return !isLlama(type); }

void stand(ServerEntity& horse, std::int32_t ticks) {
    HorseState& state = stateOf(horse);
    state.eating = false;
    state.standing = true;
    state.standCounter = ticks;
}

void standIfPossible(ServerEntity& horse) {
    // parity: twenty ticks up on its hind legs.
    constexpr std::int32_t kStandTicks = 20;
    if (canRear(horse.type())) {
        stand(horse, kStandTicks);
    }
}

void openMouth(ServerEntity& horse) {
    HorseState& state = stateOf(horse);
    state.mouthCounter = 1;
    state.mouthOpen = true;
}

std::string_view angrySound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "horse") {
        return baby(horse) ? "minecraft:entity.baby_horse.angry" : "minecraft:entity.horse.angry";
    }
    if (path == "donkey") {
        return "minecraft:entity.donkey.angry";
    }
    if (path == "mule") {
        return "minecraft:entity.mule.angry";
    }
    if (isLlama(horse.type())) {
        return "minecraft:entity.llama.angry";
    }
    if (path == "zombie_horse") {
        return "minecraft:entity.zombie_horse.angry";
    }
    return {};
}

std::string_view eatingSound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "horse") {
        return baby(horse) ? "minecraft:entity.baby_horse.eat" : "minecraft:entity.horse.eat";
    }
    if (path == "donkey") {
        return "minecraft:entity.donkey.eat";
    }
    if (path == "mule") {
        return "minecraft:entity.mule.eat";
    }
    if (isLlama(horse.type())) {
        return "minecraft:entity.llama.eat";
    }
    if (path == "zombie_horse") {
        return "minecraft:entity.zombie_horse.eat";
    }
    if (path == "camel") {
        return "minecraft:entity.camel.eat";
    }
    if (path == "camel_husk") {
        return "minecraft:entity.camel_husk.eat";
    }
    return {};
}

std::string_view ambientSound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "horse") {
        return baby(horse) ? "minecraft:entity.baby_horse.ambient" : "minecraft:entity.horse.ambient";
    }
    if (path == "donkey") {
        return "minecraft:entity.donkey.ambient";
    }
    if (path == "mule") {
        return "minecraft:entity.mule.ambient";
    }
    if (isLlama(horse.type())) {
        return "minecraft:entity.llama.ambient";
    }
    if (path == "zombie_horse") {
        return "minecraft:entity.zombie_horse.ambient";
    }
    if (path == "skeleton_horse") {
        return "minecraft:entity.skeleton_horse.ambient";
    }
    return {};
}

std::string_view jumpSound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "donkey") {
        return "minecraft:entity.donkey.jump";
    }
    if (path == "mule") {
        return "minecraft:entity.mule.jump";
    }
    return "minecraft:entity.horse.jump";
}

std::string_view chestSound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "mule") {
        return "minecraft:entity.mule.chest";
    }
    return isLlama(horse.type()) ? "minecraft:entity.llama.chest" : "minecraft:entity.donkey.chest";
}

std::string_view saddleSound(const ServerEntity& horse) {
    const std::string_view path = horse.type().path();
    if (path == "camel") {
        return "minecraft:entity.camel.saddle";
    }
    if (path == "camel_husk") {
        return "minecraft:entity.camel_husk.saddle";
    }
    return "minecraft:entity.horse.saddle";
}

float voicePitch(const ServerEntity& horse, float roll) { return (roll * 0.2F) + (baby(horse) ? 1.5F : 1.0F); }

} // namespace basalt::server::runtime::entity::horses
