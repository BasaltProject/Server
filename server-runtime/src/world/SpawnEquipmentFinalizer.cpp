#include <basalt/server/runtime/world/SpawnEquipment.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr float kArmorChance = 0.15F;
constexpr float kArmorUpgradeChance = 0.1087F;
constexpr std::int32_t kArmorUpgradeAttempts = 3;
constexpr std::int32_t kInitialArmorTiers = 3;
constexpr float kPartialArmorChance = 0.25F;
constexpr float kHardPartialArmorChance = 0.1F;
constexpr float kWeaponEnchantChance = 0.25F;
constexpr float kArmorEnchantChance = 0.5F;
constexpr float kPickupLootChance = 0.55F;
constexpr float kPiglinArmorChance = 0.1F;
constexpr float kZombieWeaponChance = 0.01F;
constexpr float kHardZombieWeaponChance = 0.05F;
constexpr float kDrownedWeaponRoll = 0.9F;
constexpr std::int32_t kDrownedWeaponBound = 16;
constexpr std::int32_t kDrownedTridentResults = 10;
constexpr std::int32_t kZombieWeaponBound = 6;
constexpr std::int32_t kZombifiedPiglinSpearOdds = 20;
constexpr std::int32_t kPillagerPiercingOdds = 300;
constexpr float kHalloweenHeadChance = 0.25F;
constexpr float kJackOLanternChance = 0.1F;
constexpr float kGuaranteedDropChance = 2.0F;

constexpr std::array<std::string_view, 5> kAbstractSkeletonTypes{"skeleton", "stray", "bogged", "parched",
                                                                 "wither_skeleton"};
constexpr std::array<std::string_view, 2> kSkeletonTrapEnchantmentOrder{"mainhand", "head"};
constexpr std::array<std::string_view, 4> kArmorPopulationOrder{"head", "chest", "legs", "feet"};
constexpr std::array<std::string_view, 4> kArmorEnchantmentOrder{"feet", "legs", "chest", "head"};
constexpr std::array<std::pair<std::string_view, std::string_view>, 4> kPiglinArmor{
    {{"head", "golden_helmet"}, {"chest", "golden_chestplate"}, {"legs", "golden_leggings"}, {"feet", "golden_boots"}}};
constexpr std::array<std::array<std::string_view, 4>, 6> kArmorTiers{
    {{"leather_helmet", "leather_chestplate", "leather_leggings", "leather_boots"},
     {"copper_helmet", "copper_chestplate", "copper_leggings", "copper_boots"},
     {"golden_helmet", "golden_chestplate", "golden_leggings", "golden_boots"},
     {"chainmail_helmet", "chainmail_chestplate", "chainmail_leggings", "chainmail_boots"},
     {"iron_helmet", "iron_chestplate", "iron_leggings", "iron_boots"},
     {"diamond_helmet", "diamond_chestplate", "diamond_leggings", "diamond_boots"}}};

[[nodiscard]] NbtCompound compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    const auto* compound = found == nullptr ? nullptr : found->get<NbtCompound>();
    return compound == nullptr ? NbtCompound{} : *compound;
}

[[nodiscard]] NbtCompound itemAt(const NbtCompound& data, std::string_view slot) {
    return compoundAt(compoundAt(data, "equipment"), slot);
}

void writeItem(NbtCompound& data, std::string_view slot, NbtCompound stack) {
    NbtCompound equipment = compoundAt(data, "equipment");
    equipment.set(slot, NbtTag::ofCompound(std::move(stack)));
    data.set("equipment", NbtTag::ofCompound(std::move(equipment)));
}

[[nodiscard]] bool isEmpty(const NbtCompound& stack) { return stack.names().empty(); }

[[nodiscard]] bool isDefaultHalloween() {
    const std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    return today.month() == std::chrono::October && today.day() == std::chrono::day{31};
}

} // namespace

SpawnEquipmentFinalizer::SpawnEquipmentFinalizer(const SpawnEquipmentJar& jar, IsHalloween isHalloween)
    : enchantments_(jar), isHalloween_(isHalloween ? std::move(isHalloween) : IsHalloween{isDefaultHalloween}) {}

void SpawnEquipmentFinalizer::setItem(NbtCompound& data, std::string_view slot, std::string_view item) const {
    NbtCompound stack;
    stack.set("id", NbtTag::ofString("minecraft:" + std::string{item}));
    stack.putInt("count", 1);
    writeItem(data, slot, std::move(stack));
}

std::optional<std::string> SpawnEquipmentFinalizer::itemId(const NbtCompound& data, std::string_view slot) const {
    const NbtCompound stack = itemAt(data, slot);
    const NbtTag* found = stack.find("id");
    const auto* name = found == nullptr ? nullptr : found->get<std::string>();
    return name == nullptr || name->empty() ? std::nullopt : std::optional{*name};
}

void SpawnEquipmentFinalizer::setGuaranteedDrop(NbtCompound& data, std::string_view slot) const {
    NbtCompound chances = compoundAt(data, "drop_chances");
    chances.set(slot, NbtTag::ofFloat(kGuaranteedDropChance));
    data.set("drop_chances", NbtTag::ofCompound(std::move(chances)));
}

void SpawnEquipmentFinalizer::enchantSlot(NbtCompound& data, std::string_view slot, random::RandomSource& random,
                                          float chance, float specialMultiplier) const {
    NbtCompound stack = itemAt(data, slot);
    if (isEmpty(stack) || random.nextFloat() >= chance * specialMultiplier) {
        return;
    }
    enchantments_.enchant(stack, random, specialMultiplier);
    writeItem(data, slot, std::move(stack));
}

void SpawnEquipmentFinalizer::populateArmor(NbtCompound& data, random::RandomSource& random, float specialMultiplier,
                                            std::string_view difficulty) const {
    if (random.nextFloat() >= kArmorChance * specialMultiplier) {
        return;
    }
    std::int32_t tier = random.nextInt(kInitialArmorTiers);
    for (std::int32_t attempt = 0; attempt < kArmorUpgradeAttempts; attempt++) {
        if (random.nextFloat() < kArmorUpgradeChance) {
            tier++;
        }
    }
    const float partialChance = difficulty == "hard" ? kHardPartialArmorChance : kPartialArmorChance;
    for (std::size_t index = 0; index < kArmorPopulationOrder.size(); index++) {
        if (index > 0 && random.nextFloat() < partialChance) {
            break;
        }
        if (isEmpty(itemAt(data, kArmorPopulationOrder[index]))) {
            setItem(data, kArmorPopulationOrder[index], kArmorTiers[static_cast<std::size_t>(tier)][index]);
        }
    }
}

void SpawnEquipmentFinalizer::addHalloweenHead(NbtCompound& data, random::RandomSource& random) const {
    if (!isEmpty(itemAt(data, "head")) || !isHalloween_() || random.nextFloat() >= kHalloweenHeadChance) {
        return;
    }
    setItem(data, "head", random.nextFloat() < kJackOLanternChance ? "jack_o_lantern" : "carved_pumpkin");
    // parity: the head a mob is wearing for Halloween never drops, unlike the rest of its gear.
    NbtCompound chances = compoundAt(data, "drop_chances");
    chances.set("head", NbtTag::ofFloat(0.0F));
    data.set("drop_chances", NbtTag::ofCompound(std::move(chances)));
}

void SpawnEquipmentFinalizer::finalizeAfterCommon(NbtCompound& data, std::string_view type,
                                                  random::RandomSource& random, float specialMultiplier,
                                                  std::string_view difficulty) const {
    if (std::ranges::find(kAbstractSkeletonTypes, type) == kAbstractSkeletonTypes.end()) {
        return;
    }
    finalizeSkeleton(data, type, random, specialMultiplier, difficulty);
}

void SpawnEquipmentFinalizer::finalizeSkeleton(NbtCompound& data, std::string_view type, random::RandomSource& random,
                                               float specialMultiplier, std::string_view difficulty) const {
    if (type == "wither_skeleton") {
        setItem(data, "mainhand", "stone_sword");
    } else {
        populateArmor(data, random, specialMultiplier, difficulty);
        setItem(data, "mainhand", "bow");
        enchantSlot(data, "mainhand", random, kWeaponEnchantChance, specialMultiplier);
        for (const std::string_view slot : kArmorEnchantmentOrder) {
            enchantSlot(data, slot, random, kArmorEnchantChance, specialMultiplier);
        }
    }
    data.putBoolean("CanPickUpLoot", random.nextFloat() < kPickupLootChance * specialMultiplier);
    addHalloweenHead(data, random);
}

void SpawnEquipmentFinalizer::finalizePiglinEquipmentBeforeCommon(NbtCompound& data, random::RandomSource& random,
                                                                  float specialMultiplier, bool baby) const {
    if (!baby) {
        for (const auto& [slot, item] : kPiglinArmor) {
            if (random.nextFloat() < kPiglinArmorChance) {
                setItem(data, slot, item);
            }
        }
    }
    enchantSlot(data, "mainhand", random, kWeaponEnchantChance, specialMultiplier);
    for (const std::string_view slot : kArmorEnchantmentOrder) {
        enchantSlot(data, slot, random, kArmorEnchantChance, specialMultiplier);
    }
}

void SpawnEquipmentFinalizer::finalizePillagerEquipmentBeforeCommon(NbtCompound& data, random::RandomSource& random,
                                                                    float specialMultiplier) const {
    setItem(data, "mainhand", "crossbow");
    enchantSlot(data, "mainhand", random, kWeaponEnchantChance, specialMultiplier);
    if (random.nextInt(kPillagerPiercingOdds) != 0) {
        return;
    }
    NbtCompound crossbow = itemAt(data, "mainhand");
    NbtCompound components = compoundAt(crossbow, "components");
    NbtCompound levels = compoundAt(components, "minecraft:enchantments");
    const NbtTag* current = levels.find("minecraft:piercing");
    const auto* held = current == nullptr ? nullptr : current->get<std::int32_t>();
    levels.putInt("minecraft:piercing", std::max(1, held == nullptr ? 0 : *held));
    components.set("minecraft:enchantments", NbtTag::ofCompound(std::move(levels)));
    crossbow.set("components", NbtTag::ofCompound(std::move(components)));
    writeItem(data, "mainhand", std::move(crossbow));
}

void SpawnEquipmentFinalizer::finalizeZombieEquipmentAfterCommon(NbtCompound& data, std::string_view type,
                                                                 random::RandomSource& random, float specialMultiplier,
                                                                 std::string_view difficulty) const {
    if (type == "drowned") {
        if (random.nextFloat() > kDrownedWeaponRoll) {
            setItem(data, "mainhand",
                    random.nextInt(kDrownedWeaponBound) < kDrownedTridentResults ? "trident" : "fishing_rod");
        }
    } else if (type == "zombified_piglin") {
        setItem(data, "mainhand", random.nextInt(kZombifiedPiglinSpearOdds) == 0 ? "golden_spear" : "golden_sword");
    } else {
        populateArmor(data, random, specialMultiplier, difficulty);
        const float weaponChance = difficulty == "hard" ? kHardZombieWeaponChance : kZombieWeaponChance;
        if (random.nextFloat() < weaponChance) {
            switch (random.nextInt(kZombieWeaponBound)) {
            case 0:
                setItem(data, "mainhand", "iron_sword");
                break;
            case 1:
                setItem(data, "mainhand", "iron_spear");
                break;
            default:
                setItem(data, "mainhand", "iron_shovel");
                break;
            }
        }
    }
    enchantSlot(data, "mainhand", random, kWeaponEnchantChance, specialMultiplier);
    for (const std::string_view slot : kArmorEnchantmentOrder) {
        enchantSlot(data, slot, random, kArmorEnchantChance, specialMultiplier);
    }
    addHalloweenHead(data, random);
}

void SpawnEquipmentFinalizer::finalizeSkeletonTrapEquipment(NbtCompound& data, random::RandomSource& random,
                                                            float specialMultiplier) const {
    if (isEmpty(itemAt(data, "head"))) {
        setItem(data, "head", "iron_helmet");
    }
    for (const std::string_view slot : kSkeletonTrapEnchantmentOrder) {
        NbtCompound stack = itemAt(data, slot);
        NbtCompound components = compoundAt(stack, "components");
        static_cast<void>(components.remove("minecraft:enchantments"));
        stack.set("components", NbtTag::ofCompound(std::move(components)));
        enchantments_.enchant(stack, random, specialMultiplier);
        writeItem(data, slot, std::move(stack));
    }
}

const SpawnEquipmentFinalizer& spawnEquipmentWithoutAssets() {
    static const SpawnEquipmentFinalizer fallback{SpawnEquipmentJar{}};
    return fallback;
}

} // namespace basalt::server::runtime::world
