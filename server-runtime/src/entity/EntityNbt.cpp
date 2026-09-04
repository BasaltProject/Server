#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>

#include <algorithm>
#include <array>
#include <expected>
#include <set>
#include <string>
#include <utility>

namespace basalt::server::runtime::entity::entityNbt {
namespace {

using wire::common::Identifier;
using wire::common::Uuid;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kMaxHealthAttribute = "minecraft:max_health";
constexpr std::string_view kLastSleptMemory = "minecraft:last_slept";
constexpr std::string_view kGolemDetectedMemory = "minecraft:golem_detected_recently";

constexpr std::array<std::string_view, 6> kPatrollingTypes{"pillager",   "vindicator", "evoker",
                                                           "illusioner", "witch",      "ravager"};

[[nodiscard]] bool patrols(std::string_view path) {
    return std::ranges::find(kPatrollingTypes, path) != kPatrollingTypes.end();
}

} // namespace

double numericDouble(const NbtCompound& tag, std::string_view key) {
    const NbtTag* value = tag.find(key);
    if (value == nullptr) {
        return 0.0;
    }
    if (const std::int8_t* asByte = value->get<std::int8_t>(); asByte != nullptr) {
        return static_cast<double>(*asByte);
    }
    if (const std::int16_t* asShort = value->get<std::int16_t>(); asShort != nullptr) {
        return static_cast<double>(*asShort);
    }
    if (const std::int32_t* asInt = value->get<std::int32_t>(); asInt != nullptr) {
        return static_cast<double>(*asInt);
    }
    if (const std::int64_t* asLong = value->get<std::int64_t>(); asLong != nullptr) {
        return static_cast<double>(*asLong);
    }
    if (const float* asFloat = value->get<float>(); asFloat != nullptr) {
        return static_cast<double>(*asFloat);
    }
    const double* asDouble = value->get<double>();
    return asDouble == nullptr ? 0.0 : *asDouble;
}

std::int32_t numericInt(const NbtCompound& tag, std::string_view key) {
    return static_cast<std::int32_t>(numericDouble(tag, key));
}

namespace {

[[nodiscard]] bool numericBoolean(const NbtCompound& tag, std::string_view key) {
    return numericDouble(tag, key) != 0.0;
}

[[nodiscard]] NbtTag doubles(double first, double second, double third) {
    NbtList list{NbtType::Double};
    list.push(NbtTag::ofDouble(first));
    list.push(NbtTag::ofDouble(second));
    list.push(NbtTag::ofDouble(third));
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] NbtTag floats(float first, float second) {
    NbtList list{NbtType::Float};
    list.push(NbtTag::ofFloat(first));
    list.push(NbtTag::ofFloat(second));
    return NbtTag::ofList(std::move(list));
}

/// A reference rather than a copy: a `string_view` read off a returned temporary would dangle the
/// moment the statement ended, which is a bug that only shows up as an empty field.
[[nodiscard]] const NbtCompound& compoundOf(const NbtCompound& tag, std::string_view key) {
    static const NbtCompound empty;
    const NbtCompound* found = tag.getCompound(key);
    return found == nullptr ? empty : *found;
}

void writeVillagerGolemMemories(NbtCompound& tag, const ServerEntity& entity) {
    NbtCompound brain = compoundOf(tag, "Brain");
    NbtCompound memories = compoundOf(brain, "memories");
    if (entity.lastSleptGameTime != std::numeric_limits<std::int64_t>::min()) {
        NbtCompound slept;
        slept.putLong("value", entity.lastSleptGameTime);
        memories.set(kLastSleptMemory, NbtTag::ofCompound(std::move(slept)));
    }
    if (entity.golemDetectedTicks > 0) {
        NbtCompound detected;
        detected.putBoolean("value", true);
        detected.putLong("ttl", static_cast<std::int64_t>(entity.golemDetectedTicks));
        memories.set(kGolemDetectedMemory, NbtTag::ofCompound(std::move(detected)));
    } else {
        memories.remove(kGolemDetectedMemory);
    }
    brain.set("memories", NbtTag::ofCompound(std::move(memories)));
    tag.set("Brain", NbtTag::ofCompound(std::move(brain)));
}

void readVillagerGolemMemories(const NbtCompound& tag, ServerEntity& entity) {
    const NbtCompound& memories = compoundOf(compoundOf(tag, "Brain"), "memories");
    const NbtCompound& slept = compoundOf(memories, kLastSleptMemory);
    if (slept.contains("value")) {
        entity.lastSleptGameTime = static_cast<std::int64_t>(numericDouble(slept, "value"));
    }
    const NbtCompound& detected = compoundOf(memories, kGolemDetectedMemory);
    if (numericBoolean(detected, "value")) {
        entity.golemDetectedTicks = numericInt(detected, "ttl");
    }
}

/// Reads Minecraft 26.2's `AttributeInstance.Packed.LIST_CODEC` representation.
void readAttributes(const NbtCompound& tag, ServerEntity& entity) {
    const NbtList* attributes = tag.getList("attributes");
    if (attributes == nullptr) {
        return;
    }
    for (const NbtTag& element : *attributes) {
        const NbtCompound* attribute = element.get<NbtCompound>();
        if (attribute == nullptr) {
            continue;
        }
        const std::string id{attribute->getString("id")};
        if (id.empty()) {
            continue;
        }
        if (attribute->contains("base")) {
            entity.attributeValues.insert_or_assign(id, numericDouble(*attribute, "base"));
        }
        const NbtList* modifiers = attribute->getList("modifiers");
        if (modifiers == nullptr) {
            continue;
        }
        for (const NbtTag& modifierTag : *modifiers) {
            const NbtCompound* packed = modifierTag.get<NbtCompound>();
            if (packed == nullptr) {
                continue;
            }
            const std::string modifierId{packed->getString("id")};
            const std::optional<AttributeOperation> operation =
                attributeOperationByName(packed->getString("operation"));
            if (modifierId.empty() || !operation.has_value()) {
                continue;
            }
            entity.attributeModifiers[id].insert_or_assign(
                modifierId, AttributeModifier{
                                .id = modifierId, .amount = numericDouble(*packed, "amount"), .operation = *operation});
        }
    }
}

NbtCompound encodeTreeNode(const ServerEntity& entity, const EntityById& entityById, std::set<std::int32_t>& visited) {
    visited.insert(entity.entityId());
    NbtCompound tag = encode(entity);
    NbtList passengers{NbtType::Compound};
    for (const std::int32_t passengerId : entity.passengerIds) {
        ServerEntity* passenger = entityById ? entityById(passengerId) : nullptr;
        if (passenger == nullptr || passenger->removed || !visited.insert(passenger->entityId()).second) {
            continue;
        }
        passengers.push(NbtTag::ofCompound(encodeTreeNode(*passenger, entityById, visited)));
    }
    if (passengers.empty()) {
        tag.remove("Passengers");
    } else {
        tag.set("Passengers", NbtTag::ofList(std::move(passengers)));
    }
    return tag;
}

ServerEntity* decodeTreeNode(const NbtCompound& tag, ServerEntity* vehicle,
                             std::vector<std::unique_ptr<ServerEntity>>& entities,
                             const AllocateEntityId& allocateEntityId, const ProtocolTypeOf& protocolTypeOf) {
    const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(tag.getString("id"));
    if (!type.has_value() || !protocolTypeOf || !allocateEntityId) {
        return nullptr;
    }
    const std::optional<std::int32_t> protocolType = protocolTypeOf(*type);
    if (!protocolType.has_value()) {
        return nullptr;
    }
    std::unique_ptr<ServerEntity> decoded = decode(tag, allocateEntityId(), *protocolType);
    if (decoded == nullptr) {
        return nullptr;
    }
    ServerEntity* entity = decoded.get();
    if (vehicle != nullptr) {
        entity->vehicleId = vehicle->entityId();
        vehicle->passengerIds.push_back(entity->entityId());
    }
    entities.push_back(std::move(decoded));
    if (const NbtList* passengers = tag.getList("Passengers"); passengers != nullptr) {
        for (const NbtTag& element : *passengers) {
            if (const NbtCompound* passenger = element.get<NbtCompound>(); passenger != nullptr) {
                (void)decodeTreeNode(*passenger, entity, entities, allocateEntityId, protocolTypeOf);
            }
        }
    }
    return entity;
}

ServerEntity* decodeGeneratedTreeNode(const server::world::ChunkGeneratedEntity& descriptor, ServerEntity* vehicle,
                                      std::vector<std::unique_ptr<ServerEntity>>& entities,
                                      const AllocateEntityId& allocateEntityId, const ProtocolTypeOf& protocolTypeOf) {
    if (!protocolTypeOf || !allocateEntityId) {
        return nullptr;
    }
    const std::optional<std::int32_t> protocolType = protocolTypeOf(descriptor.type);
    if (!protocolType.has_value()) {
        return nullptr;
    }
    std::unique_ptr<ServerEntity> decoded = decodeGenerated(descriptor, allocateEntityId(), *protocolType);
    if (decoded == nullptr) {
        return nullptr;
    }
    ServerEntity* entity = decoded.get();
    if (vehicle != nullptr) {
        entity->vehicleId = vehicle->entityId();
        vehicle->passengerIds.push_back(entity->entityId());
    }
    entities.push_back(std::move(decoded));
    for (const server::world::ChunkGeneratedEntity& passenger : descriptor.passengers) {
        (void)decodeGeneratedTreeNode(passenger, entity, entities, allocateEntityId, protocolTypeOf);
    }
    return entity;
}

} // namespace

NbtCompound encode(const ServerEntity& entity) {
    NbtCompound tag = entity.sourceNbt.has_value() ? *entity.sourceNbt : NbtCompound{};
    tag.remove("Passengers");
    tag.putString("id", entity.type().toString());
    tag.putString("UUID", entity.uuid().toString());
    tag.set("Pos", doubles(entity.x, entity.y, entity.z));
    tag.set("Motion", doubles(entity.velocityX, entity.velocityY, entity.velocityZ));
    tag.set("Rotation", floats(entity.yaw, entity.pitch));
    // parity: Air is `Entity.saveWithoutId`, so everything has one. Each field below belongs to a
    // class further down, and writing one on an entity that has no such field leaves a tag vanilla
    // never writes: a boat with a Health, an arrow with an Age.
    tag.putShort("Air", static_cast<std::int16_t>(entity.airSupply));
    const std::string_view path = entity.type().path();
    if (server::registry::entityAttributes::isLiving(entity.type())) {
        tag.putFloat("Health", static_cast<float>(entity.health));
    }
    if (mobs::isMob(entity.type())) {
        tag.putBoolean("PersistenceRequired", entity.persistent);
        tag.putBoolean("NoAI", entity.noAi);
        tag.putBoolean("CanPickUpLoot", entity.canPickUpLoot);
    }
    if (ageableMobs::isAgeable(path)) {
        tag.putInt("Age", entity.age);
    }
    if (!breeding::foodFor(entity.type()).empty()) {
        tag.putInt("LoveTicks", entity.loveTicks);
        tag.putInt("BreedCooldown", entity.breedCooldown);
    }
    if (taming::isTamable(entity.type())) {
        tag.putBoolean("Sitting", entity.sitting);
    }
    if (path == "tnt") {
        // parity: `PrimedTnt.TAG_FUSE` is lowercase; `Fuse` is a creeper's own swell time.
        tag.putShort("fuse", static_cast<std::int16_t>(entity.fuse));
    }
    if (cubeMobs::isCube(entity.type())) {
        tag.putInt("Size", std::max(entity.cubeSize, 1) - 1);
    }
    if (patrols(path)) {
        if (entity.patrolTarget.has_value()) {
            tag.putIntArray("patrol_target", {entity.patrolTarget->begin(), entity.patrolTarget->end()});
        } else {
            tag.remove("patrol_target");
        }
        tag.putBoolean("PatrolLeader", entity.patrolLeader);
        tag.putBoolean("Patrolling", entity.patrolling);
    }
    if (path == "wandering_trader" || path == "trader_llama") {
        tag.putInt("DespawnDelay", entity.despawnDelay);
    }
    if (path == "wandering_trader") {
        if (entity.wanderTarget.has_value()) {
            tag.putIntArray("wander_target", {entity.wanderTarget->begin(), entity.wanderTarget->end()});
        } else {
            tag.remove("wander_target");
        }
    }
    if (path == "skeleton_horse") {
        tag.putBoolean("SkeletonTrap", entity.skeletonTrap);
        tag.putInt("SkeletonTrapTime", entity.skeletonTrapTime);
        tag.putBoolean("Tame", entity.tamedHorse);
    }
    if (entity.leashHolderUuid.has_value()) {
        NbtCompound leash;
        leash.putString("UUID", *entity.leashHolderUuid);
        tag.set("leash", NbtTag::ofCompound(std::move(leash)));
    }
    if (path == "villager" || path == "zombie_villager") {
        NbtCompound villagerData = compoundOf(tag, "VillagerData");
        if (villagerData.getString("type").empty()) {
            villagerData.putString("type", "minecraft:plains");
        }
        villagerData.putString("profession", "minecraft:" + entity.profession);
        villagerData.putInt("level", entity.villagerLevel);
        tag.set("VillagerData", NbtTag::ofCompound(std::move(villagerData)));
        tag.putInt("Xp", entity.villagerExperience);
    }
    if (path == "villager") {
        writeVillagerGolemMemories(tag, entity);
    }
    if (entity.owner.has_value()) {
        tag.putString("Owner", *entity.owner);
    }
    if (entity.customName.has_value()) {
        tag.putString("CustomName", *entity.customName);
    }
    if (!entity.tags.empty()) {
        NbtList tags{NbtType::String};
        for (const std::string& name : entity.tags) {
            tags.push(NbtTag::ofString(name));
        }
        tag.set("Tags", NbtTag::ofList(std::move(tags)));
    }
    return tag;
}

NbtCompound encodeTree(const ServerEntity& root, const EntityById& entityById) {
    std::set<std::int32_t> visited;
    return encodeTreeNode(root, entityById, visited);
}

std::unique_ptr<ServerEntity> decode(const NbtCompound& tag, std::int32_t entityId, std::int32_t protocolType) {
    const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(tag.getString("id"));
    if (!type.has_value()) {
        return nullptr;
    }
    const NbtList* pos = tag.getList("Pos");
    if (pos == nullptr || pos->size() < 3) {
        return nullptr;
    }
    std::array<double, 3> position{};
    for (std::size_t index = 0; index < 3; index++) {
        const double* value = (*pos)[index].get<double>();
        if (value == nullptr) {
            return nullptr;
        }
        position[index] = *value;
    }
    std::array<float, 2> rotation{0.0F, 0.0F};
    if (const NbtList* stored = tag.getList("Rotation"); stored != nullptr) {
        for (std::size_t index = 0; index < std::min<std::size_t>(2, stored->size()); index++) {
            if (const float* value = (*stored)[index].get<float>(); value != nullptr) {
                rotation[index] = *value;
            }
        }
    }
    const std::optional<Uuid> uuid = Uuid::parse(tag.getString("UUID"));
    auto entity = std::make_unique<ServerEntity>(entityId, uuid.value_or(Uuid::random()), *type, protocolType,
                                                 position[0], position[1], position[2], rotation[0], rotation[1]);

    if (const NbtList* motion = tag.getList("Motion"); motion != nullptr && motion->size() >= 3) {
        const double* velocityX = (*motion)[0].get<double>();
        const double* velocityY = (*motion)[1].get<double>();
        const double* velocityZ = (*motion)[2].get<double>();
        if (velocityX != nullptr && velocityY != nullptr && velocityZ != nullptr) {
            entity->velocityX = *velocityX;
            entity->velocityY = *velocityY;
            entity->velocityZ = *velocityZ;
        }
    }
    if (const double health = numericDouble(tag, "Health"); health > 0.0) {
        entity->health = health;
    }
    const std::string_view path = type->path();
    entity->age = ageableMobs::usesBooleanBabyFlag(path) && numericBoolean(tag, "IsBaby") ? ageableMobs::kBabyStartAge
                                                                                          : numericInt(tag, "Age");
    if (tag.contains("fuse")) {
        entity->fuse = numericInt(tag, "fuse");
    }
    if (tag.contains("Air")) {
        entity->airSupply = static_cast<std::int32_t>(tag.getShort("Air"));
    }
    entity->cubeSize =
        cubeMobs::isCube(*type) ? std::clamp(numericInt(tag, "Size") + 1, 1, 127) : cubeMobs::kDefaultSize;
    entity->persistent = numericBoolean(tag, "PersistenceRequired");
    entity->noAi = numericBoolean(tag, "NoAI");
    entity->canPickUpLoot = tag.getBoolean("CanPickUpLoot");
    entity->loveTicks = numericInt(tag, "LoveTicks");
    entity->breedCooldown = numericInt(tag, "BreedCooldown");
    if (patrols(path)) {
        if (const std::span<const std::int32_t> target = tag.getIntArray("patrol_target"); target.size() >= 3) {
            entity->patrolTarget = std::array<std::int32_t, 3>{target[0], target[1], target[2]};
        }
        entity->patrolLeader = numericBoolean(tag, "PatrolLeader");
        entity->patrolling = numericBoolean(tag, "Patrolling");
    }
    if ((path == "wandering_trader" || path == "trader_llama") && tag.contains("DespawnDelay")) {
        entity->despawnDelay = tag.getInt("DespawnDelay");
    }
    if (path == "wandering_trader") {
        if (const std::span<const std::int32_t> target = tag.getIntArray("wander_target"); target.size() >= 3) {
            entity->wanderTarget = std::array<std::int32_t, 3>{target[0], target[1], target[2]};
        }
    }
    if (path == "skeleton_horse") {
        entity->skeletonTrap = numericBoolean(tag, "SkeletonTrap");
        entity->skeletonTrapTime = numericInt(tag, "SkeletonTrapTime");
        entity->tamedHorse = numericBoolean(tag, "Tame");
    }
    if (const std::string_view leash = compoundOf(tag, "leash").getString("UUID"); !leash.empty()) {
        entity->leashHolderUuid = std::string{leash};
    }
    const NbtCompound& villagerData = compoundOf(tag, "VillagerData");
    if (const std::string_view profession = villagerData.getString("profession"); !profession.empty()) {
        const std::size_t colon = profession.find(':');
        entity->profession = std::string{colon == std::string_view::npos ? profession : profession.substr(colon + 1)};
    }
    if (!villagerData.empty()) {
        entity->villagerLevel = std::max(villagerData.getInt("level", 1), 1);
    }
    if (tag.contains("Xp")) {
        entity->villagerExperience = tag.getInt("Xp");
    }
    if (path == "villager") {
        readVillagerGolemMemories(tag, *entity);
    }
    if (const std::string_view owner = tag.getString("Owner"); !owner.empty()) {
        entity->owner = std::string{owner};
    }
    entity->sitting = numericBoolean(tag, "Sitting");
    if (const std::string_view customName = tag.getString("CustomName"); !customName.empty()) {
        entity->customName = std::string{customName};
    }
    if (const NbtList* tags = tag.getList("Tags"); tags != nullptr) {
        for (const NbtTag& element : *tags) {
            if (const std::string* name = element.get<std::string>(); name != nullptr) {
                entity->tags.push_back(*name);
            }
        }
    }
    readAttributes(tag, *entity);
    bool hasPackedMaxHealth = false;
    if (const NbtList* attributes = tag.getList("attributes"); attributes != nullptr) {
        for (const NbtTag& element : *attributes) {
            const NbtCompound* attribute = element.get<NbtCompound>();
            if (attribute != nullptr && attribute->getString("id") == kMaxHealthAttribute) {
                hasPackedMaxHealth = true;
            }
        }
    }
    if (hasPackedMaxHealth) {
        entity->health = std::min(entity->health, entity->attributeWithModifiers(kMaxHealthAttribute));
    }
    entity->sourceNbt = tag;
    return entity;
}

std::vector<std::unique_ptr<ServerEntity>> decodeTree(const NbtCompound& tag, const AllocateEntityId& allocateEntityId,
                                                      const ProtocolTypeOf& protocolTypeOf) {
    std::vector<std::unique_ptr<ServerEntity>> entities;
    (void)decodeTreeNode(tag, nullptr, entities, allocateEntityId, protocolTypeOf);
    return entities;
}

std::unique_ptr<ServerEntity> decodeGenerated(const server::world::ChunkGeneratedEntity& descriptor,
                                              std::int32_t entityId, std::int32_t protocolType) {
    NbtCompound tag = descriptor.data;
    tag.putString("id", descriptor.type.toString());
    tag.set("Pos", doubles(descriptor.x, descriptor.y, descriptor.z));
    tag.set("Rotation", floats(descriptor.yaw, descriptor.pitch));
    return decode(tag, entityId, protocolType);
}

std::vector<std::unique_ptr<ServerEntity>> decodeGeneratedTree(const server::world::ChunkGeneratedEntity& descriptor,
                                                               const AllocateEntityId& allocateEntityId,
                                                               const ProtocolTypeOf& protocolTypeOf) {
    std::vector<std::unique_ptr<ServerEntity>> entities;
    (void)decodeGeneratedTreeNode(descriptor, nullptr, entities, allocateEntityId, protocolTypeOf);
    return entities;
}

} // namespace basalt::server::runtime::entity::entityNbt
