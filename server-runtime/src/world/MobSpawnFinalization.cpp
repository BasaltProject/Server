#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/MobSpawnAttributes.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world::mobSpawnFinalization {
namespace {

namespace reg = server::registry;

using entity::AttributeOperation;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

constexpr float kBabySpawnChance = 0.05F;
constexpr std::int32_t kForcedBabyGroupIndex = 2;
constexpr float kLeftHandedChance = 0.05F;
constexpr double kFollowRangeBonusSpread = 0.11485000000000001;
constexpr std::string_view kFollowRangeAttribute = "minecraft:follow_range";
constexpr std::string_view kRandomSpawnBonusId = "minecraft:random_spawn_bonus";
constexpr std::string_view kMaxHealthAttribute = "minecraft:max_health";
constexpr std::int32_t kMaxNaturalSizeScale = 2;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] NbtCompound packedAttribute(std::string_view id, double base,
                                          const std::vector<NbtCompound>& modifiers = {}) {
    NbtCompound attribute;
    attribute.set("id", NbtTag::ofString(std::string{id}));
    attribute.set("base", NbtTag::ofDouble(base));
    if (!modifiers.empty()) {
        std::vector<NbtTag> tags;
        tags.reserve(modifiers.size());
        for (const NbtCompound& modifier : modifiers) {
            tags.push_back(NbtTag::ofCompound(modifier));
        }
        attribute.set("modifiers", NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, std::move(tags)}));
    }
    return attribute;
}

void writeTypeState(NbtCompound& data, const CommonSpawnState& state) {
    for (const std::string& key : state.typeData.names()) {
        if (const NbtTag* value = state.typeData.find(key); value != nullptr) {
            data.set(key, *value);
        }
    }
}

/// Writes the vanilla fields that let generated mobs survive materialization and chunk saves.
void writeMobState(NbtCompound& data, const Identifier& type, const CommonSpawnState& state) {
    if (!state.ranMobFinalizer) {
        return;
    }
    data.putBoolean("LeftHanded", state.leftHanded);
    std::vector<NbtCompound> packed;
    for (const auto& [id, base] : state.attributeBases) {
        packed.push_back(packedAttribute(id, base));
    }
    if (const std::optional<double> base = reg::entityAttributes::baseValue(type, reg::entityAttributes::kFollowRange);
        base.has_value()) {
        NbtCompound modifier;
        modifier.set("id", NbtTag::ofString(std::string{kRandomSpawnBonusId}));
        modifier.set("amount", NbtTag::ofDouble(state.followRangeBonus));
        modifier.set("operation", NbtTag::ofString(std::string{
                                      entity::attributeOperationName(AttributeOperation::AddMultipliedBase)}));
        packed.push_back(packedAttribute(kFollowRangeAttribute, *base, {modifier}));
    }
    if (!packed.empty()) {
        std::vector<NbtTag> tags;
        tags.reserve(packed.size());
        for (NbtCompound& attribute : packed) {
            tags.push_back(NbtTag::ofCompound(std::move(attribute)));
        }
        data.set("attributes", NbtTag::ofList(NbtList{wire::nbt::NbtType::Compound, std::move(tags)}));
    }
}

[[nodiscard]] std::int16_t shortAt(const NbtCompound& data, std::string_view key) {
    const NbtTag* found = data.find(key);
    const auto* value = found == nullptr ? nullptr : found->get<std::int16_t>();
    return value == nullptr ? 0 : *value;
}

[[nodiscard]] bool booleanAt(const NbtCompound& data, std::string_view key) {
    const NbtTag* found = data.find(key);
    const auto* byte = found == nullptr ? nullptr : found->get<std::int8_t>();
    return byte != nullptr && *byte != 0;
}

} // namespace

std::optional<float> babySpawnChance(std::string_view type) {
    // These concrete classes construct `AgeableMobGroupData(false)`, and axolotls and foxes replace
    // the roll with "member three onward", so neither has a chance of its own.
    static constexpr std::array<std::string_view, 9> kNoRoll{
        "magma_cube", "parrot", "slime", "strider", "villager", "wandering_trader", "wolf", "axolotl", "fox"};
    if (std::ranges::find(kNoRoll, type) != kNoRoll.end()) {
        return std::nullopt;
    }
    if (type == "dolphin") {
        return 0.1F;
    }
    if (type == "donkey" || type == "mule" || type == "panda" || type == "zombie_horse") {
        return 0.2F;
    }
    if (type == "ocelot" || type == "polar_bear" || type == "rabbit") {
        return 1.0F;
    }
    return kBabySpawnChance;
}

CommonSpawnState stateFor(const Identifier& type, std::int32_t groupIndex, const MobSpawnContext& context,
                          MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                          random::RandomSource& entityRandom) {
    random::RandomSource& random = *context.random;
    CommonSpawnState state;
    if (type.path() == "cave_spider") {
        // parity: `CaveSpider.finalizeSpawn` deliberately answers group data without calling `Mob`
        // at all, so nothing below it runs and nothing below it is written.
        state.cubeSize = entity::cubeMobs::kDefaultSize;
        return state;
    }

    const bool concreteBaby = mobVariantSpawnFinalizer::beforeCommon(type.path(), context, group, state.typeData,
                                                                     blockX, blockY, blockZ, entityRandom);
    mobSpawnAttributes::write(type.path(), random, state.typeData, state.attributeBases);

    const std::optional<float> babyChance = babySpawnChance(type.path());
    bool inheritedBaby = false;
    if (entity::ageableMobs::isAgeable(type) && groupIndex > 0) {
        if (type.path() == "axolotl" || type.path() == "fox") {
            inheritedBaby = groupIndex >= kForcedBabyGroupIndex;
        } else if (babyChance.has_value()) {
            inheritedBaby = random.nextFloat() <= *babyChance;
        }
    }
    state.age =
        (concreteBaby || inheritedBaby) && type.path() != "zombie_nautilus" ? entity::ageableMobs::kBabyStartAge : 0;
    // parity: `Mob.finalizeSpawn`. `RandomSource.triangle` consumes two doubles before the
    // left-handed float, even though neither result usually changes what the client draws.
    state.followRangeBonus = random.triangle(0.0, kFollowRangeBonusSpread);
    state.leftHanded = random.nextFloat() < kLeftHandedChance;
    state.ranMobFinalizer = true;

    if (!entity::cubeMobs::isCube(type)) {
        mobVariantSpawnFinalizer::afterCommon(type.path(), group, random, state.typeData);
        state.cubeSize = entity::cubeMobs::kDefaultSize;
        return state;
    }
    std::int32_t sizeScale = random.nextInt(kMaxNaturalSizeScale + 1);
    if (sizeScale < kMaxNaturalSizeScale && random.nextFloat() < 0.5F * context.specialMultiplier) {
        sizeScale++;
    }
    state.cubeSize = 1 << sizeScale;
    return state;
}

void writeGeneratedNode(MobSpawnForestBuilder& builder, MobSpawnNode& node, std::int32_t groupIndex,
                        const MobSpawnContext& context, MobSpawnGroupData& group, std::int32_t blockX,
                        std::int32_t blockY, std::int32_t blockZ, random::RandomSource& entityRandom) {
    const Identifier type = node.type();
    equineSpawnFinalizer::beforeCommon(builder, node, context);
    striderSpawnFinalizer::beforeCommon(builder, node, context);

    const CommonSpawnState state = stateFor(type, groupIndex, context, group, blockX, blockY, blockZ, entityRandom);
    NbtCompound& data = node.data();
    if (state.age != 0) {
        data.putInt("Age", state.age);
    }
    writeTypeState(data, state);
    writeMobState(data, type, state);
    context.equipmentOrFallback().finalizeAfterCommon(data, type.path(), *context.random, context.specialMultiplier,
                                                      context.difficulty);
    zombieSpawnFinalizer::afterCommon(builder, node, group, context, entityRandom);
    // parity: `AbstractCubeMob` saves one less than its live size, so absent or zero means size one.
    if (entity::cubeMobs::isCube(type)) {
        data.putInt("Size", state.cubeSize - 1);
    }
    mobJockeyFinalization::afterCommon(builder, node, groupIndex, context, group);
}

std::vector<server::world::ChunkGeneratedEntity> apply(entity::ServerEntity& entity, std::int32_t groupIndex,
                                                       const MobSpawnContext& context, MobSpawnGroupData& group) {
    random::RandomSource entityRandom = context.entityRandom();
    const CommonSpawnState state = stateFor(entity.type(), groupIndex, context, group, blockOf(entity.x),
                                            blockOf(entity.y), blockOf(entity.z), entityRandom);
    entity.age = state.age;
    if (entity.type().path() == "dolphin") {
        entity.pitch = 0.0F;
    }
    for (const auto& [id, base] : state.attributeBases) {
        entity.attributeValues.insert_or_assign(id, base);
    }
    if (const auto found = state.attributeBases.find(std::string{kMaxHealthAttribute});
        found != state.attributeBases.end()) {
        entity.health = std::min(entity.health, found->second);
    }
    if (state.ranMobFinalizer) {
        entity.attributeModifiers[std::string{kFollowRangeAttribute}].insert_or_assign(
            std::string{kRandomSpawnBonusId},
            entity::AttributeModifier{.id = std::string{kRandomSpawnBonusId},
                                      .amount = state.followRangeBonus,
                                      .operation = AttributeOperation::AddMultipliedBase});
    }

    if (!entity.sourceNbt.has_value()) {
        entity.sourceNbt = NbtCompound{};
    }
    NbtCompound& data = *entity.sourceNbt;
    writeTypeState(data, state);
    context.equipmentOrFallback().finalizeAfterCommon(data, entity.type().path(), *context.random,
                                                      context.specialMultiplier, context.difficulty);
    if (state.typeData.find("Air") != nullptr) {
        entity.airSupply = shortAt(state.typeData, "Air");
    }
    if (data.find("CanPickUpLoot") != nullptr) {
        entity.canPickUpLoot = booleanAt(data, "CanPickUpLoot");
    }
    writeMobState(data, entity.type(), state);
    entity::cubeMobs::applySize(entity, state.cubeSize);

    MobSpawnForestBuilder builder;
    MobSpawnNode& main = builder.create(entity.type(), entity.x, entity.y, entity.z, entity.yaw, 0.0F, data);
    mobJockeyFinalization::afterCommon(builder, main, groupIndex, context, group);
    return builder.freeze(main).main.passengers;
}

std::vector<server::world::ChunkGeneratedEntity>
writeGenerated(NbtCompound& data, const Identifier& type, std::int32_t groupIndex, const MobSpawnContext& context,
               MobSpawnGroupData& group, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ, double entityX,
               double entityY, double entityZ, float entityYaw) {
    const MobSpawnForest forest = writeGeneratedForest(data, type, groupIndex, context, group, blockX, blockY, blockZ,
                                                       entityX, entityY, entityZ, entityYaw);
    return forest.main.passengers;
}

} // namespace basalt::server::runtime::world::mobSpawnFinalization

namespace basalt::server::runtime::world {

MobSpawnForest writeGeneratedForest(wire::nbt::NbtCompound& data, const wire::common::Identifier& type,
                                    std::int32_t groupIndex, const MobSpawnContext& context, MobSpawnGroupData& group,
                                    std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ, double entityX,
                                    double entityY, double entityZ, float entityYaw, MobSpawnEnvironment environment) {
    MobSpawnForestBuilder builder{std::move(environment)};
    MobSpawnNode& main = builder.create(type, entityX, entityY, entityZ, entityYaw, 0.0F, data);
    random::RandomSource entityRandom = context.entityRandom();
    mobSpawnFinalization::writeGeneratedNode(builder, main, groupIndex, context, group, blockX, blockY, blockZ,
                                             entityRandom);
    // The caller handed in the NBT and expects to see what the finalizers wrote into it.
    data = main.data();
    return builder.freeze(main);
}

} // namespace basalt::server::runtime::world
