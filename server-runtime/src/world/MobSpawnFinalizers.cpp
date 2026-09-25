#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/MobSpawnVariantData.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] std::string registryKey(std::string_view path) { return "minecraft:" + std::string{path}; }

[[nodiscard]] NbtCompound compoundAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    const auto* compound = found == nullptr ? nullptr : found->get<NbtCompound>();
    return compound == nullptr ? NbtCompound{} : *compound;
}

[[nodiscard]] std::int32_t intAt(const NbtCompound& root, std::string_view key) {
    const NbtTag* found = root.find(key);
    const auto* value = found == nullptr ? nullptr : found->get<std::int32_t>();
    return value == nullptr ? 0 : *value;
}

} // namespace

const SpawnEquipmentFinalizer& MobSpawnContext::equipmentOrFallback() const {
    return equipment == nullptr ? spawnEquipmentWithoutAssets() : *equipment;
}

random::RandomSource MobSpawnContext::entityRandom() const {
    return entityRandomFactory ? entityRandomFactory() : random::RandomSource::legacy(0);
}

std::string_view MobSpawnContext::biomePath() const {
    return biome.has_value() ? std::string_view{biome->path()} : std::string_view{};
}

namespace catSpawnFinalizer {
namespace {

constexpr float kFullMoonThreshold = 0.9F;
constexpr std::int32_t kAllBlackIndex = 10;
constexpr std::array<std::string_view, 11> kVariants{"tabby",    "black",   "red",     "siamese", "british_shorthair",
                                                     "calico",   "persian", "ragdoll", "white",   "jellie",
                                                     "all_black"};
constexpr std::array<std::string_view, 2> kSoundVariants{"classic", "royal"};

} // namespace

void afterCommon(const CatSpawnContext& context, random::RandomSource& random, NbtCompound& data) {
    std::string_view variant;
    if (context.inBlackCatStructure) {
        // parity: `PriorityProvider` still samples the one-element highest-priority result.
        static_cast<void>(random.nextInt(1));
        variant = kVariants[kAllBlackIndex];
    } else {
        const std::int32_t candidates =
            context.moonBrightness >= kFullMoonThreshold ? static_cast<std::int32_t>(kVariants.size()) : kAllBlackIndex;
        variant = kVariants[static_cast<std::size_t>(random.nextInt(candidates))];
    }
    data.set("variant", NbtTag::ofString(registryKey(variant)));
    data.set("sound_variant", NbtTag::ofString(registryKey(kSoundVariants[static_cast<std::size_t>(
                                  random.nextInt(static_cast<std::int32_t>(kSoundVariants.size())))])));
}

} // namespace catSpawnFinalizer

namespace piglinSpawnFinalizer {
namespace {

constexpr float kBabyChance = 0.2F;
constexpr float kCrossbowChance = 0.5F;
constexpr std::int32_t kGoldenSpearOdds = 10;
constexpr std::int32_t kMinHuntDelay = 30 * 20;
constexpr std::int32_t kHuntDelayRange = (120 - 30) * 20 + 1;

} // namespace

bool finalizeBeforeCommon(NbtCompound& data, const MobSpawnContext& context, random::RandomSource& entityRandom) {
    random::RandomSource& random = *context.random;
    const SpawnEquipmentFinalizer& equipment = context.equipmentOrFallback();
    const bool baby = context.reason != MobSpawnReason::Structure && random.nextFloat() < kBabyChance;
    data.putBoolean("IsBaby", baby);
    if (context.reason != MobSpawnReason::Structure && !baby) {
        std::string_view weapon = "golden_sword";
        if (entityRandom.nextFloat() < kCrossbowChance) {
            weapon = "crossbow";
        } else if (entityRandom.nextInt(kGoldenSpearOdds) == 0) {
            weapon = "golden_spear";
        }
        equipment.setItem(data, "mainhand", weapon);
    }

    const std::int32_t huntDelay = kMinHuntDelay + random.nextInt(kHuntDelayRange);
    NbtCompound hunted;
    hunted.putBoolean("value", true);
    hunted.putLong("ttl", huntDelay);
    NbtCompound memories;
    memories.set("minecraft:hunted_recently", NbtTag::ofCompound(std::move(hunted)));
    NbtCompound brain;
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    data.set("Brain", NbtTag::ofCompound(std::move(brain)));
    equipment.finalizePiglinEquipmentBeforeCommon(data, random, context.specialMultiplier, baby);
    return baby;
}

} // namespace piglinSpawnFinalizer

namespace villagerSpawnFinalizer {
namespace {

[[nodiscard]] const std::set<std::string, std::less<>>& desertBiomes() {
    static const std::set<std::string, std::less<>> all{"badlands", "desert", "eroded_badlands", "wooded_badlands"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& snowBiomes() {
    static const std::set<std::string, std::less<>> all{
        "deep_frozen_ocean", "frozen_ocean", "frozen_river", "ice_spikes",   "snowy_beach", "snowy_taiga",
        "snowy_plains",      "grove",        "snowy_slopes", "frozen_peaks", "jagged_peaks"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& swampBiomes() {
    static const std::set<std::string, std::less<>> all{"swamp", "mangrove_swamp"};
    return all;
}

[[nodiscard]] const std::set<std::string, std::less<>>& taigaBiomes() {
    static const std::set<std::string, std::less<>> all{
        "old_growth_spruce_taiga", "old_growth_pine_taiga", "windswept_gravelly_hills", "windswept_hills", "taiga",
        "windswept_forest"};
    return all;
}

[[nodiscard]] std::string_view villagerType(std::string_view biome) {
    if (desertBiomes().contains(biome)) {
        return "desert";
    }
    if (mobSpawnVariantData::jungleBiomes().contains(biome)) {
        return "jungle";
    }
    if (mobSpawnVariantData::savannaBiomes().contains(biome)) {
        return "savanna";
    }
    if (snowBiomes().contains(biome)) {
        return "snow";
    }
    if (swampBiomes().contains(biome)) {
        return "swamp";
    }
    if (taigaBiomes().contains(biome)) {
        return "taiga";
    }
    return "plains";
}

} // namespace

const std::vector<std::string>& professions() {
    static const std::vector<std::string> all{"none",   "armorer",   "butcher",  "cartographer",  "cleric",
                                              "farmer", "fisherman", "fletcher", "leatherworker", "librarian",
                                              "mason",  "nitwit",    "shepherd", "toolsmith",     "weaponsmith"};
    return all;
}

void beforeCommon(std::string_view type, std::string_view biome, MobSpawnReason reason,
                  random::RandomSource& entityRandom, NbtCompound& data) {
    if (type != "villager" && type != "zombie_villager") {
        return;
    }
    // parity: `ZombieVillager` initializes its profession from its own entity RNG in the
    // constructor, before `finalizeSpawn` selects the biome-derived villager type.
    const std::string profession = type == "zombie_villager"
                                       ? professions()[static_cast<std::size_t>(
                                             entityRandom.nextInt(static_cast<std::int32_t>(professions().size())))]
                                       : "none";
    NbtCompound villagerData;
    villagerData.set("type", NbtTag::ofString(registryKey(villagerType(biome))));
    villagerData.set("profession", NbtTag::ofString(registryKey(profession)));
    villagerData.putInt("level", 1);
    data.set("VillagerData", NbtTag::ofCompound(std::move(villagerData)));
    data.putBoolean("VillagerDataFinalized", true);
    if (type == "villager" && reason == MobSpawnReason::Structure) {
        data.putBoolean("AssignProfessionWhenSpawned", true);
    }
}

} // namespace villagerSpawnFinalizer

namespace pillagerSpawnFinalizer {
namespace {

constexpr float kRandomLeaderChance = 0.06F;
constexpr std::int32_t kPatrolTargetRadius = 500;
constexpr std::int32_t kPatrolTargetDiameter = 1000;

constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kOminousPatterns{
    {{"rhombus", "cyan"},
     {"stripe_bottom", "light_gray"},
     {"stripe_center", "gray"},
     {"border", "light_gray"},
     {"stripe_middle", "black"},
     {"half_horizontal", "light_gray"},
     {"circle", "light_gray"},
     {"border", "black"}}};

void writeOminousBanner(NbtCompound& data, const SpawnEquipmentFinalizer& equipment) {
    equipment.setItem(data, "head", "white_banner");
    equipment.setGuaranteedDrop(data, "head");
    NbtCompound equipmentTag = compoundAt(data, "equipment");
    NbtCompound banner = compoundAt(equipmentTag, "head");
    NbtCompound components = compoundAt(banner, "components");

    std::vector<NbtTag> patterns;
    for (const auto& [pattern, colour] : kOminousPatterns) {
        NbtCompound entry;
        entry.set("pattern", NbtTag::ofString(registryKey(pattern)));
        entry.set("color", NbtTag::ofString(std::string{colour}));
        patterns.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    components.set("minecraft:banner_patterns",
                   NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, std::move(patterns)}));

    NbtCompound tooltip;
    tooltip.set("hidden_components",
                NbtTag::ofList(NbtList{wire::nbt::NbtType::String, {NbtTag::ofString("minecraft:banner_patterns")}}));
    components.set("minecraft:tooltip_display", NbtTag::ofCompound(std::move(tooltip)));

    NbtCompound name;
    name.set("translate", NbtTag::ofString("block.minecraft.ominous_banner"));
    components.set("minecraft:item_name", NbtTag::ofCompound(std::move(name)));
    components.set("minecraft:rarity", NbtTag::ofString("uncommon"));

    banner.set("components", NbtTag::ofCompound(std::move(components)));
    equipmentTag.set("head", NbtTag::ofCompound(std::move(banner)));
    data.set("equipment", NbtTag::ofCompound(std::move(equipmentTag)));
}

} // namespace

void beforeCommon(MobSpawnGroupData& group, const MobSpawnContext& context, random::RandomSource& entityRandom,
                  NbtCompound& data) {
    random::RandomSource& random = *context.random;
    const SpawnEquipmentFinalizer& equipment = context.equipmentOrFallback();
    equipment.finalizePillagerEquipmentBeforeCommon(data, random, context.specialMultiplier);

    bool leader = group.patrolLeader;
    if (!leader && context.reason != MobSpawnReason::Patrol && context.reason != MobSpawnReason::Event &&
        context.reason != MobSpawnReason::Structure) {
        leader = random.nextFloat() < kRandomLeaderChance;
    }
    if (group.patrolLeader) {
        // parity: `PatrolSpawner.findPatrolTarget` runs before `setPos`, while the fresh entity is
        // still at the origin, and consumes the entity's own RNG rather than the level RNG.
        data.putIntArray("patrol_target", {-kPatrolTargetRadius + entityRandom.nextInt(kPatrolTargetDiameter), 0,
                                           -kPatrolTargetRadius + entityRandom.nextInt(kPatrolTargetDiameter)});
    }
    if (leader) {
        data.putBoolean("PatrolLeader", true);
        data.putBoolean("Patrolling", true);
        writeOminousBanner(data, equipment);
    } else if (context.reason == MobSpawnReason::Patrol) {
        data.putBoolean("Patrolling", true);
    }
    // parity: `Raider.finalizeSpawn`; only naturally spawned witches disable this flag.
    data.putBoolean("CanJoinRaid", true);
}

} // namespace pillagerSpawnFinalizer

namespace equineSpawnFinalizer {

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& horse, const MobSpawnContext& context) {
    if (horse.type().path() != "zombie_horse" || context.reason != MobSpawnReason::Natural) {
        return;
    }
    // parity: `ZombieHorse.finalizeSpawn` creates and fully finalizes the zombie before
    // `AbstractHorse` randomizes the horse, then replaces the rider's main-hand item.
    MobSpawnNode& rider = builder.create(Identifier::vanilla("zombie"), horse.x(), horse.y(), horse.z(), horse.yaw());
    MobSpawnGroupData group;
    random::RandomSource entityRandom = context.entityRandom();
    mobSpawnFinalization::writeGeneratedNode(builder, rider, 0, context, group, blockOf(rider.x()), blockOf(rider.y()),
                                             blockOf(rider.z()), entityRandom);
    context.equipmentOrFallback().setItem(rider.data(), "mainhand", "iron_spear");
    static_cast<void>(builder.mount(rider, horse));
}

} // namespace equineSpawnFinalizer

namespace striderSpawnFinalizer {
namespace {

constexpr std::int32_t kZombifiedPiglinJockeyOdds = 30;
constexpr std::int32_t kBabyStriderJockeyOdds = 10;
constexpr float kZombieBabyChance = 0.05F;
constexpr std::int32_t kBabyStartAge = -24000;

void finalizeJockey(MobSpawnForestBuilder& builder, MobSpawnNode& jockey, MobSpawnGroupData& group,
                    const MobSpawnContext& context) {
    MobSpawnContext jockeyContext = context;
    jockeyContext.reason = MobSpawnReason::Jockey;
    random::RandomSource entityRandom = context.entityRandom();
    mobSpawnFinalization::writeGeneratedNode(builder, jockey, 0, jockeyContext, group, blockOf(jockey.x()),
                                             blockOf(jockey.y()), blockOf(jockey.z()), entityRandom);
}

} // namespace

void beforeCommon(MobSpawnForestBuilder& builder, MobSpawnNode& strider, const MobSpawnContext& context) {
    if (strider.type().path() != "strider" || intAt(strider.data(), "Age") < 0) {
        return;
    }
    random::RandomSource& random = *context.random;
    const SpawnEquipmentFinalizer& equipment = context.equipmentOrFallback();
    if (random.nextInt(kZombifiedPiglinJockeyOdds) == 0) {
        MobSpawnGroupData group;
        group.zombieBaby = random.nextFloat() < kZombieBabyChance;
        group.zombieCanSpawnJockey = false;
        MobSpawnNode& jockey = builder.create(Identifier::vanilla("zombified_piglin"), strider.x(), strider.y(),
                                              strider.z(), strider.yaw());
        finalizeJockey(builder, jockey, group, context);
        equipment.setItem(jockey.data(), "mainhand", "warped_fungus_on_a_stick");
        equipment.setItem(strider.data(), "saddle", "saddle");
        equipment.setGuaranteedDrop(strider.data(), "saddle");
        static_cast<void>(builder.mount(jockey, strider));
    } else if (random.nextInt(kBabyStriderJockeyOdds) == 0) {
        MobSpawnNode& jockey =
            builder.create(Identifier::vanilla("strider"), strider.x(), strider.y(), strider.z(), strider.yaw());
        jockey.data().putInt("Age", kBabyStartAge);
        MobSpawnGroupData group;
        finalizeJockey(builder, jockey, group, context);
        static_cast<void>(builder.mount(jockey, strider));
    }
}

} // namespace striderSpawnFinalizer

namespace mobJockeyFinalization {
namespace {

constexpr std::int32_t kSpiderJockeyBound = 100;
constexpr float kSpiderEffectChance = 0.1F;
constexpr std::array<std::string_view, 5> kSpiderEffects{"speed", "speed", "strength", "regeneration", "invisibility"};

void writeInfiniteEffect(NbtCompound& data, const std::string& effect) {
    NbtCompound instance;
    instance.set("id", NbtTag::ofString(registryKey(effect)));
    instance.putInt("duration", -1);
    instance.putBoolean("show_icon", true);
    data.set("active_effects",
             NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, {NbtTag::ofCompound(std::move(instance))}}));
    if (effect == "invisibility") {
        data.putBoolean("Invisible", true);
    }
}

} // namespace

void afterCommon(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                 const MobSpawnContext& context, MobSpawnGroupData& group) {
    if (node.type().path() != "spider") {
        return;
    }
    random::RandomSource& random = *context.random;
    if (random.nextInt(kSpiderJockeyBound) == 0) {
        MobSpawnNode& passenger =
            builder.create(Identifier::vanilla("skeleton"), node.x(), node.y(), node.z(), node.yaw());
        MobSpawnContext jockeyContext = context;
        jockeyContext.reason = MobSpawnReason::Jockey;
        MobSpawnGroupData own;
        random::RandomSource entityRandom = context.entityRandom();
        mobSpawnFinalization::writeGeneratedNode(builder, passenger, 0, jockeyContext, own, blockOf(node.x()),
                                                 blockOf(node.y()), blockOf(node.z()), entityRandom);
        static_cast<void>(builder.mount(passenger, node));
    }
    if (groupIndex == 0) {
        group.spiderEffect.reset();
        if (context.difficulty == "hard" && random.nextFloat() < kSpiderEffectChance * context.specialMultiplier) {
            group.spiderEffect = std::string{kSpiderEffects[static_cast<std::size_t>(
                random.nextInt(static_cast<std::int32_t>(kSpiderEffects.size())))]};
        }
    }
    if (group.spiderEffect.has_value()) {
        writeInfiniteEffect(node.data(), *group.spiderEffect);
    }
}

} // namespace mobJockeyFinalization

} // namespace basalt::server::runtime::world
