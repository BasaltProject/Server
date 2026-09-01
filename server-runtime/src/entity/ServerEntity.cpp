#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
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
    airSupply = dolphin ? kDolphinMaxAir : kDefaultMaxAir;
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
    // Against what the additions left, not against the running total: that is what makes the order
    // of the three operations matter.
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

double ServerEntity::width() const { return registry::entityTypeShapes::widthOf(type_); }

double ServerEntity::height() const { return registry::entityTypeShapes::heightOf(type_); }

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
