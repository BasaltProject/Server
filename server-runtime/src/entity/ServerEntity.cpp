#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::entity {
namespace {

constexpr std::array<std::pair<std::string_view, double>, 5> kAttributeDefaults{{
    {attribute::kMaxHealth, 20.0},
    {attribute::kMovementSpeed, 0.7},
    {attribute::kAttackDamage, 2.0},
    {attribute::kArmor, 0.0},
    {attribute::kScale, 1.0},
}};

constexpr std::array<std::pair<AttributeOperation, std::string_view>, 3> kOperationNames{{
    {AttributeOperation::AddValue, "add_value"},
    {AttributeOperation::AddMultipliedBase, "add_multiplied_base"},
    {AttributeOperation::AddMultipliedTotal, "add_multiplied_total"},
}};

constexpr std::array<std::pair<MobCategory, MobCategoryFacts>, 8> kCategories{{
    {MobCategory::Monster,
     {.categoryName = "monster", .maxPerChunk = 70, .friendly = false, .persistent = false, .despawnDistance = 128}},
    {MobCategory::Creature,
     {.categoryName = "creature", .maxPerChunk = 10, .friendly = true, .persistent = true, .despawnDistance = 128}},
    {MobCategory::Ambient,
     {.categoryName = "ambient", .maxPerChunk = 15, .friendly = true, .persistent = false, .despawnDistance = 128}},
    {MobCategory::Axolotls,
     {.categoryName = "axolotls", .maxPerChunk = 5, .friendly = true, .persistent = false, .despawnDistance = 128}},
    {MobCategory::UndergroundWaterCreature,
     {.categoryName = "underground_water_creature",
      .maxPerChunk = 5,
      .friendly = true,
      .persistent = false,
      .despawnDistance = 128}},
    {MobCategory::WaterCreature,
     {.categoryName = "water_creature",
      .maxPerChunk = 5,
      .friendly = true,
      .persistent = false,
      .despawnDistance = 128}},
    {MobCategory::WaterAmbient,
     {.categoryName = "water_ambient",
      .maxPerChunk = 20,
      .friendly = true,
      .persistent = false,
      .despawnDistance = 64}},
    {MobCategory::Misc,
     {.categoryName = "misc", .maxPerChunk = -1, .friendly = true, .persistent = true, .despawnDistance = 128}},
}};

} // namespace

std::string_view attributeOperationName(AttributeOperation operation) noexcept {
    for (const auto& [candidate, name] : kOperationNames) {
        if (candidate == operation) {
            return name;
        }
    }
    return kOperationNames.front().second;
}

std::optional<AttributeOperation> attributeOperationByName(std::string_view name) {
    for (const auto& [operation, candidate] : kOperationNames) {
        if (candidate == name) {
            return operation;
        }
    }
    return std::nullopt;
}

std::optional<double> attribute::defaultFor(std::string_view id) {
    for (const auto& [name, value] : kAttributeDefaults) {
        if (name == id) {
            return value;
        }
    }
    return std::nullopt;
}

const MobCategoryFacts& mobCategoryFacts(MobCategory category) noexcept {
    for (const auto& [candidate, facts] : kCategories) {
        if (candidate == category) {
            return facts;
        }
    }
    return kCategories.back().second;
}

std::optional<MobCategory> mobCategoryByName(std::string_view name) {
    for (const auto& [category, facts] : kCategories) {
        if (facts.categoryName == name) {
            return category;
        }
    }
    return std::nullopt;
}

ServerEntity::ServerEntity(std::int32_t entityId, wire::common::Uuid uuid, wire::common::Identifier type,
                           std::int32_t protocolType, double x, double y, double z, float yaw, float pitch)
    : x(x), y(y), z(z), yaw(yaw), pitch(pitch), entityId_(entityId), uuid_(uuid), type_(std::move(type)),
      protocolType_(protocolType) {
    const bool dolphin = type_.path() == "dolphin";
    airSupply = mobBreathing::maxAirOf(type_.path());
    canPickUpLoot = dolphin;
    despawnDelay = type_.path() == "trader_llama" ? kTraderLlamaDespawnDelay : 0;
    const std::optional<double> maxHealth =
        registry::entityAttributes::baseValue(type_, registry::entityAttributes::kMaxHealth);
    health = maxHealth.value_or(kDefaultMaxHealth);
}

double ServerEntity::attribute(std::string_view id) const {
    const auto set = attributeValues.find(id);
    if (set != attributeValues.end()) {
        return set->second;
    }
    if (const std::optional<double> born = registry::entityAttributes::baseValue(type_, id); born.has_value()) {
        return *born;
    }
    return attribute::defaultFor(id).value_or(0.0);
}

double ServerEntity::attributeWithModifiers(std::string_view id) const {
    const double base = attribute(id);
    const auto found = attributeModifiers.find(id);
    if (found == attributeModifiers.end()) {
        return base;
    }

    double value = base;
    for (const auto& [name, modifier] : found->second) {
        if (modifier.operation == AttributeOperation::AddValue) {
            value += modifier.amount;
        }
    }
    // Against what the additions left, not the running total, which is what makes operation order matter.
    const double afterAdditions = value;
    for (const auto& [name, modifier] : found->second) {
        if (modifier.operation == AttributeOperation::AddMultipliedBase) {
            value += afterAdditions * modifier.amount;
        }
    }
    for (const auto& [name, modifier] : found->second) {
        if (modifier.operation == AttributeOperation::AddMultipliedTotal) {
            value *= 1.0 + modifier.amount;
        }
    }
    return value;
}

namespace {

/// parity: `Camel`'s own baby boxes.
constexpr float kBabyCamelWidth = 0.95F;
constexpr float kBabyCamelHeight = 1.4F;
constexpr float kBabyCamelSittingHeight = 0.425F;
/// parity: `ADULT_SITTING_DIMENSIONS`, the type's height less this.
constexpr float kCamelSittingDrop = 1.43F;
/// parity: `Sniffer.DIGGING_DIMENSIONS`, the type's height less this, scaled by the age.
constexpr float kSnifferDiggingDrop = 0.4F;

[[nodiscard]] bool babyCamel(const ServerEntity& entity) { return entity.camel != nullptr && entity.age < 0; }

struct BabyBox {
    std::string_view type;
    float width = 0.0F;
    float height = 0.0F;
    float scale = 0.0F;
};

/// parity: every `BABY_DIMENSIONS` a `getDefaultDimensions` returns for a baby.
constexpr std::array<BabyBox, 36> kBabyBoxes{{
    {"villager", 0.49F, 0.98F},
    {"zombie_villager", 0.49F, 0.98F},
    {"zombie", 0.49F, 0.98F},
    {"husk", 0.49F, 0.98F},
    {"drowned", 0.49F, 0.98F},
    {"piglin", 0.49F, 0.98F},
    {"zombified_piglin", 0.49F, 0.98F},
    {"strider", 0.45F, 0.85F},
    {"hoglin", 0.75F, 0.85F},
    {"zoglin", 0.75F, 0.85F},
    {"polar_bear", 0.7F, 0.7F},
    {"squid", 0.5F, 0.5F},
    {"glow_squid", 0.5F, 0.5F},
    {"axolotl", 0.375F, 0.21F},
    {"pig", 0.45F, 0.45F},
    {"cow", 0.45F, 0.7F},
    {"mooshroom", 0.45F, 0.7F},
    {"goat", 0.45F, 0.65F},
    {"rabbit", 0.24F, 0.4F},
    {"cat", 0.3F, 0.35F},
    {"ocelot", 0.3F, 0.35F},
    {"wolf", 0.3F, 0.425F},
    {"sheep", 0.45F, 0.65F},
    {"chicken", 0.3F, 0.4F},
    {"horse", 0.0F, 0.0F, 0.7F},
    {"skeleton_horse", 0.0F, 0.0F, 0.7F},
    {"zombie_horse", 0.0F, 0.0F, 0.7F},
    {"donkey", 0.0F, 0.0F, 0.5F},
    {"mule", 0.0F, 0.0F, 0.5F},
    {"llama", 0.0F, 0.0F, 0.5F},
    {"trader_llama", 0.0F, 0.0F, 0.5F},
    {"dolphin", 0.0F, 0.0F, 0.65F},
    {"armadillo", 0.0F, 0.0F, 0.6F},
    {"panda", 0.0F, 0.0F, 0.5F},
    {"fox", 0.0F, 0.0F, 0.6F},
    {"turtle", 0.0F, 0.0F, 0.3F},
}};

/// parity: `LivingEntity.getAgeScale`, `HappyGhast` and `Nautilus`'s `BABY_DIMENSIONS`.
[[nodiscard]] float ageScaleOf(std::string_view type) { return type == "happy_ghast" ? 0.2375F : 0.5F; }

[[nodiscard]] bool baby(const ServerEntity& entity) {
    return entity.age < 0 && ageableMobs::hasBabyMetadata(entity.type().path());
}

/// parity: `getDefaultDimensions` for a baby.
[[nodiscard]] std::pair<float, float> defaultBox(const ServerEntity& entity, const wire::common::Identifier& type) {
    const auto width = static_cast<float>(registry::entityTypeShapes::widthOf(type));
    const auto height = static_cast<float>(registry::entityTypeShapes::heightOf(type));
    const std::string_view path = type.path();
    if (path == "slime" || path == "magma_cube" || path == "sulfur_cube") {
        const auto size = static_cast<float>(entity.cubeSize);
        return {width * size, height * size};
    }
    // parity: `Phantom.getDefaultDimensions`, fifteen per cent more a size.
    if (path == "phantom") {
        const std::int32_t size = entity.sourceNbt.has_value() ? entityNbt::numericInt(*entity.sourceNbt, "size") : 0;
        const float scale = 1.0F + (0.15F * static_cast<float>(size));
        return {width * scale, height * scale};
    }
    // parity: `Pufferfish.getScale`.
    if (path == "pufferfish") {
        const float scale = entity.puffState == 0 ? 0.5F : (entity.puffState == 1 ? 0.7F : 1.0F);
        return {width * scale, height * scale};
    }
    if (!baby(entity)) {
        return {width, height};
    }
    for (const BabyBox& box : kBabyBoxes) {
        if (box.type == path) {
            return box.scale > 0.0F ? std::pair{width * box.scale, height * box.scale}
                                    : std::pair{box.width, box.height};
        }
    }
    const float scale = ageScaleOf(path);
    return {width * scale, height * scale};
}

} // namespace

double ServerEntity::width() const {
    // parity: `Camel.getDefaultDimensions` and `Sniffer.getDefaultDimensions`.
    if (babyCamel(*this)) {
        return static_cast<double>(kBabyCamelWidth);
    }
    if (sniffer != nullptr && sniffer->pose == sniffers::Pose::Digging && age < 0) {
        return static_cast<double>(static_cast<float>(registry::entityTypeShapes::widthOf(type_)) * 0.5F);
    }
    return static_cast<double>(defaultBox(*this, type_).first);
}

double ServerEntity::height() const {
    if (camel != nullptr && camel->lastPoseTick < 0) {
        return babyCamel(*this) ? static_cast<double>(kBabyCamelSittingHeight)
                                : static_cast<double>(static_cast<float>(registry::entityTypeShapes::heightOf(type_)) -
                                                      kCamelSittingDrop);
    }
    if (babyCamel(*this)) {
        return static_cast<double>(kBabyCamelHeight);
    }
    if (sniffer != nullptr && sniffer->pose == sniffers::Pose::Digging) {
        const float digging = static_cast<float>(registry::entityTypeShapes::heightOf(type_)) - kSnifferDiggingDrop;
        return static_cast<double>(age < 0 ? digging * 0.5F : digging);
    }
    return static_cast<double>(defaultBox(*this, type_).second);
}

bool ServerEntity::hasTag(std::string_view tag) const { return std::ranges::find(tags, tag) != tags.end(); }

bool ServerEntity::addTag(std::string tag) {
    if (hasTag(tag)) {
        return false;
    }
    tags.push_back(std::move(tag));
    return true;
}

bool ServerEntity::removeTag(std::string_view tag) {
    const auto at = std::ranges::find(tags, tag);
    if (at == tags.end()) {
        return false;
    }
    tags.erase(at);
    return true;
}

} // namespace basalt::server::runtime::entity
