#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/MobSpawnVariantData.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world::mobVariantSpawnFinalizer {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::int32_t kHorseVariants = 7;
constexpr std::int32_t kHorseMarkings = 5;
constexpr std::int32_t kCommonAxolotlVariants = 4;
constexpr std::int64_t kCamelFullStandTicks = 53;
constexpr std::int32_t kDolphinMaxAir = 4800;
constexpr std::int32_t kFoxRed = 0;
constexpr std::int32_t kFoxSnow = 1;
constexpr float kFoxEquipmentChance = 0.2F;
constexpr std::int32_t kPandaGeneRolls = 16;
constexpr std::int32_t kFrogMinLongJumpCooldown = 100;
constexpr std::int32_t kFrogLongJumpCooldownRange = 41;
constexpr std::int32_t kGoatMinLongJumpCooldown = 600;
constexpr std::int32_t kGoatLongJumpCooldownRange = 601;
constexpr std::int32_t kGoatMinRamCooldown = 600;
constexpr std::int32_t kGoatRamCooldownRange = 5401;
constexpr double kGoatScreamingChance = 0.02;
constexpr float kGoatUnihornChance = 0.1F;
constexpr float kHoglinBabyChance = 0.2F;
constexpr std::int32_t kNautilusMinAttackCooldown = 2400;
constexpr std::int32_t kNautilusAttackCooldownRange = 1201;
constexpr double kTropicalCommonChance = 0.9;
constexpr std::int32_t kTropicalPatterns = 12;
constexpr std::int32_t kTropicalSmallPatterns = 6;
constexpr std::int32_t kDyeColors = 16;
constexpr float kLlamaMaxStrengthChance = 0.04F;
constexpr std::int32_t kLlamaVariants = 4;
constexpr std::int32_t kParrotVariants = 5;
constexpr std::int32_t kPhantomAnchorHeight = 5;
constexpr std::int32_t kSalmonSmallWeight = 30;
constexpr std::int32_t kSalmonMediumWeight = 50;
constexpr std::int32_t kSalmonTotalWeight = 95;
constexpr std::int32_t kRabbitBrown = 0;
constexpr std::int32_t kRabbitWhite = 1;
constexpr std::int32_t kRabbitBlack = 2;
constexpr std::int32_t kRabbitWhiteSplotched = 3;
constexpr std::int32_t kRabbitGold = 4;
constexpr std::int32_t kRabbitSalt = 5;
constexpr std::int32_t kDyeWhite = 0;
constexpr std::int32_t kDyePink = 6;
constexpr std::int32_t kDyeGray = 7;
constexpr std::int32_t kDyeLightGray = 8;
constexpr std::int32_t kDyeBrown = 12;
constexpr std::int32_t kDyeBlack = 15;

[[nodiscard]] std::string registryKey(std::string_view path) { return "minecraft:" + std::string{path}; }

void writeBrainMemories(NbtCompound& data, const std::vector<std::pair<std::string, std::int32_t>>& values) {
    NbtCompound memories;
    for (const auto& [key, value] : values) {
        NbtCompound holder;
        holder.putInt("value", value);
        memories.set(key, NbtTag::ofCompound(std::move(holder)));
    }
    NbtCompound brain;
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    data.set("Brain", NbtTag::ofCompound(std::move(brain)));
}

[[nodiscard]] std::int32_t packTropicalFish(std::int32_t pattern, std::int32_t baseColor, std::int32_t patternColor) {
    const std::int32_t large = pattern >= kTropicalSmallPatterns ? 1 : 0;
    return large | ((pattern % kTropicalSmallPatterns) << 8) | (baseColor << 16) | (patternColor << 24);
}

/// parity: `PriorityProvider.pick` still calls `nextInt(1)` after reducing the registry to one
/// match, so the draw happens whatever the biome turns out to be.
[[nodiscard]] std::string_view selectTemperatureVariant(std::string_view biome, random::RandomSource& random,
                                                        const std::set<std::string, std::less<>>& warm,
                                                        const std::set<std::string, std::less<>>& cold) {
    static_cast<void>(random.nextInt(1));
    if (warm.contains(biome)) {
        return "warm";
    }
    return cold.contains(biome) ? "cold" : "temperate";
}

[[nodiscard]] std::int32_t selectWolfVariant(std::string_view biome, random::RandomSource& random) {
    static_cast<void>(random.nextInt(1));
    if (mobSpawnVariantData::savannaBiomes().contains(biome)) {
        return 1;
    }
    if (biome == "grove") {
        return 2;
    }
    if (biome == "old_growth_pine_taiga") {
        return 3;
    }
    if (biome == "snowy_taiga") {
        return 4;
    }
    if (mobSpawnVariantData::jungleBiomes().contains(biome)) {
        return 5;
    }
    if (biome == "forest") {
        return 6;
    }
    if (biome == "old_growth_spruce_taiga") {
        return 7;
    }
    return mobSpawnVariantData::badlandsBiomes().contains(biome) ? 8 : 0;
}

void writeFoxEquipment(random::RandomSource& random, NbtCompound& data) {
    if (random.nextFloat() >= kFoxEquipmentChance) {
        return;
    }
    const float roll = random.nextFloat();
    std::string_view item = "feather";
    if (roll < 0.05F) {
        item = "emerald";
    } else if (roll < 0.2F) {
        item = "egg";
    } else if (roll < 0.4F) {
        item = random.nextInt(2) == 0 ? "rabbit_foot" : "rabbit_hide";
    } else if (roll < 0.6F) {
        item = "wheat";
    } else if (roll < 0.8F) {
        item = "leather";
    }
    NbtCompound stack;
    stack.set("id", NbtTag::ofString(registryKey(item)));
    stack.putInt("count", 1);
    NbtCompound equipment;
    equipment.set("mainhand", NbtTag::ofCompound(std::move(stack)));
    data.set("equipment", NbtTag::ofCompound(std::move(equipment)));
}

[[nodiscard]] std::int32_t rabbitVariant(std::string_view biome, std::int32_t roll) {
    if (mobSpawnVariantData::whiteRabbitBiomes().contains(biome)) {
        return roll < 80 ? kRabbitWhite : kRabbitWhiteSplotched;
    }
    if (biome == "desert") {
        return kRabbitGold;
    }
    if (roll < 50) {
        return kRabbitBrown;
    }
    return roll < 90 ? kRabbitSalt : kRabbitBlack;
}

/// parity: `SheepColorSpawnRules`, including the nested 499:1 default-to-pink provider.
[[nodiscard]] std::int32_t sheepColor(std::string_view biome, random::RandomSource& random) {
    const std::int32_t outer = random.nextInt(100);
    const auto rare = [&random](std::int32_t ordinary) { return random.nextInt(500) == 499 ? kDyePink : ordinary; };
    if (mobSpawnVariantData::warmFarmBiomes().contains(biome)) {
        if (outer < 5) {
            return kDyeGray;
        }
        if (outer < 10) {
            return kDyeLightGray;
        }
        if (outer < 15) {
            return kDyeWhite;
        }
        return outer < 18 ? kDyeBlack : rare(kDyeBrown);
    }
    if (mobSpawnVariantData::coldFarmBiomes().contains(biome)) {
        if (outer < 5) {
            return kDyeLightGray;
        }
        if (outer < 10) {
            return kDyeGray;
        }
        if (outer < 15) {
            return kDyeWhite;
        }
        return outer < 18 ? kDyeBrown : rare(kDyeBlack);
    }
    if (outer < 5) {
        return kDyeBlack;
    }
    if (outer < 10) {
        return kDyeGray;
    }
    if (outer < 15) {
        return kDyeLightGray;
    }
    return outer < 18 ? kDyeBrown : rare(kDyeWhite);
}

/// The group's variant, rolled once for the pack and shared by everyone in it.
[[nodiscard]] std::int32_t sharedVariant(MobSpawnGroupData& group, const std::function<std::int32_t()>& roll) {
    if (!group.variant.has_value()) {
        group.variant = roll();
    }
    return *group.variant;
}

} // namespace

bool beforeCommon(std::string_view type, const MobSpawnContext& context, MobSpawnGroupData& group, NbtCompound& data,
                  std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom) {
    random::RandomSource& random = *context.random;
    const std::string_view biome = context.biomePath();
    bool concreteBaby = false;
    villagerSpawnFinalizer::beforeCommon(type, biome, context.reason, entityRandom, data);

    if (type == "axolotl") {
        const std::int32_t first = sharedVariant(group, [&random] { return random.nextInt(kCommonAxolotlVariants); });
        if (!group.secondaryVariant.has_value()) {
            group.secondaryVariant = random.nextInt(kCommonAxolotlVariants);
        }
        data.putInt("Variant", random.nextInt(2) == 0 ? first : *group.secondaryVariant);
    } else if (type == "camel" || type == "camel_husk") {
        // parity: `Camel.finalizeSpawn` starts every natural camel in the fully standing pose.
        data.putLong("LastPoseTick", std::max<std::int64_t>(0, context.gameTime - kCamelFullStandTicks));
    } else if (type == "chicken" || type == "cow" || type == "pig") {
        data.set("variant",
                 NbtTag::ofString(registryKey(selectTemperatureVariant(
                     biome, random, mobSpawnVariantData::warmFarmBiomes(), mobSpawnVariantData::coldFarmBiomes()))));
        const std::vector<std::string>& sounds = type == "chicken" ? mobSpawnVariantData::chickenSoundVariants()
                                                 : type == "cow"   ? mobSpawnVariantData::cowSoundVariants()
                                                                   : mobSpawnVariantData::pigSoundVariants();
        data.set("sound_variant",
                 NbtTag::ofString(registryKey(
                     sounds[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(sounds.size())))])));
    } else if (type == "dolphin") {
        data.putShort("Air", static_cast<std::int16_t>(kDolphinMaxAir));
        data.putBoolean("CanPickUpLoot", true);
    } else if (type == "fox") {
        const std::int32_t biomeVariant = mobSpawnVariantData::whiteRabbitBiomes().contains(biome) ? kFoxSnow : kFoxRed;
        const std::int32_t variant = sharedVariant(group, [biomeVariant] { return biomeVariant; });
        data.set("Type", NbtTag::ofString(variant == kFoxSnow ? "snow" : "red"));
        writeFoxEquipment(random, data);
    } else if (type == "frog") {
        data.set("variant",
                 NbtTag::ofString(registryKey(selectTemperatureVariant(
                     biome, random, mobSpawnVariantData::warmFrogBiomes(), mobSpawnVariantData::coldFrogBiomes()))));
        writeBrainMemories(data, {{"minecraft:long_jump_cooldown_ticks",
                                   kFrogMinLongJumpCooldown + random.nextInt(kFrogLongJumpCooldownRange)}});
    } else if (type == "goat") {
        writeBrainMemories(
            data, {{"minecraft:long_jump_cooldown_ticks",
                    kGoatMinLongJumpCooldown + random.nextInt(kGoatLongJumpCooldownRange)},
                   {"minecraft:ram_cooldown_ticks", kGoatMinRamCooldown + random.nextInt(kGoatRamCooldownRange)}});
        data.putBoolean("IsScreamingGoat", random.nextDouble() < kGoatScreamingChance);
        bool hasLeftHorn = true;
        bool hasRightHorn = true;
        if (random.nextFloat() < kGoatUnihornChance) {
            (random.nextBoolean() ? hasLeftHorn : hasRightHorn) = false;
        }
        data.putBoolean("HasLeftHorn", hasLeftHorn);
        data.putBoolean("HasRightHorn", hasRightHorn);
    } else if (type == "horse") {
        const std::int32_t variant = sharedVariant(group, [&random] { return random.nextInt(kHorseVariants); });
        const std::int32_t markings = random.nextInt(kHorseMarkings);
        data.putInt("Variant", variant | (markings << 8));
    } else if (type == "hoglin") {
        concreteBaby = random.nextFloat() < kHoglinBabyChance;
    } else if (type == "llama" || type == "trader_llama") {
        const std::int32_t maxStrength = random.nextFloat() < kLlamaMaxStrengthChance ? 5 : 3;
        data.putInt("Strength", 1 + random.nextInt(maxStrength));
        data.putInt("Variant", sharedVariant(group, [&random] { return random.nextInt(kLlamaVariants); }));
    } else if (type == "nautilus") {
        writeBrainMemories(data, {{"minecraft:attack_target_cooldown",
                                   kNautilusMinAttackCooldown + random.nextInt(kNautilusAttackCooldownRange)}});
    } else if (type == "parrot") {
        data.putInt("Variant", random.nextInt(kParrotVariants));
    } else if (type == "phantom") {
        data.putIntArray("anchor_pos", {blockX, blockY + kPhantomAnchorHeight, blockZ});
        data.putInt("size", 0);
    } else if (type == "pillager") {
        pillagerSpawnFinalizer::beforeCommon(group, context, entityRandom, data);
    } else if (type == "piglin") {
        concreteBaby = piglinSpawnFinalizer::finalizeBeforeCommon(data, context, entityRandom);
    } else if (type == "panda") {
        data.set("MainGene", NbtTag::ofString(std::string{
                                 pandaGeneName(mobSpawnAttributes::pandaGene(random.nextInt(kPandaGeneRolls)))}));
        data.set("HiddenGene", NbtTag::ofString(std::string{
                                   pandaGeneName(mobSpawnAttributes::pandaGene(random.nextInt(kPandaGeneRolls)))}));
    } else if (type == "rabbit") {
        const std::int32_t rolled = rabbitVariant(biome, random.nextInt(100));
        data.putInt("RabbitType", sharedVariant(group, [rolled] { return rolled; }));
    } else if (type == "salmon") {
        const std::int32_t roll = random.nextInt(kSalmonTotalWeight);
        data.set("type", NbtTag::ofString(roll < kSalmonSmallWeight                         ? "small"
                                          : roll < kSalmonSmallWeight + kSalmonMediumWeight ? "medium"
                                                                                            : "large"));
    } else if (type == "sheep") {
        data.putByte("Color", static_cast<std::int8_t>(sheepColor(biome, random)));
    } else if (type == "turtle") {
        // parity: `Turtle.finalizeSpawn` anchors navigation and egg laying to its birth block.
        data.putIntArray("home_pos", {blockX, blockY, blockZ});
    } else if (type == "wolf") {
        const std::int32_t variant =
            sharedVariant(group, [biome, &random] { return selectWolfVariant(biome, random); });
        data.set("variant",
                 NbtTag::ofString(registryKey(mobSpawnVariantData::wolfVariants()[static_cast<std::size_t>(variant)])));
        const std::vector<std::string>& sounds = mobSpawnVariantData::wolfSoundVariants();
        data.set("sound_variant",
                 NbtTag::ofString(registryKey(
                     sounds[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(sounds.size())))])));
    } else if (type == "zombie_nautilus") {
        // parity: `PriorityProvider` still consumes `nextInt(1)` after choosing the highest match.
        static_cast<void>(random.nextInt(1));
        data.set("variant", NbtTag::ofString(registryKey(biome == "warm_ocean" ? "warm" : "temperate")));
        writeBrainMemories(data, {{"minecraft:attack_target_cooldown",
                                   kNautilusMinAttackCooldown + random.nextInt(kNautilusAttackCooldownRange)}});
    }
    return concreteBaby;
}

void afterCommon(std::string_view type, MobSpawnGroupData& group, random::RandomSource& random, NbtCompound& data) {
    if (type == "cat") {
        catSpawnFinalizer::afterCommon(group.catSpawnContext, random, data);
        return;
    }
    if (type != "tropical_fish") {
        return;
    }
    std::int32_t packed = 0;
    if (group.variant.has_value()) {
        packed = *group.variant;
    } else if (static_cast<double>(random.nextFloat()) < kTropicalCommonChance) {
        const std::vector<std::int32_t>& common = mobSpawnVariantData::tropicalCommonVariants();
        packed = common[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(common.size())))];
        group.variant = packed;
    } else {
        // parity: a rare fish is rolled fresh every time and never shared with the shoal.
        const std::int32_t pattern = random.nextInt(kTropicalPatterns);
        const std::int32_t baseColor = random.nextInt(kDyeColors);
        const std::int32_t patternColor = random.nextInt(kDyeColors);
        packed = packTropicalFish(pattern, baseColor, patternColor);
    }
    data.putInt("Variant", packed);
}

} // namespace basalt::server::runtime::world::mobVariantSpawnFinalizer
