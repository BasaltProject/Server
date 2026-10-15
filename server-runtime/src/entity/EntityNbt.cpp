#include "EntityNbtShared.hpp"

#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/Goats.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobStatusEffects.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Sniffers.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/entity/villager/VillagerNbt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::entity::entityNbt {
namespace {

using namespace detail;
using wire::common::Identifier;
using wire::common::Uuid;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kMaxHealthAttribute = "minecraft:max_health";

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
                             const AllocateEntityIdFor& allocateEntityId, const ProtocolTypeOf& protocolTypeOf) {
    const std::expected<Identifier, wire::common::IdentifierError> type = Identifier::parse(tag.getString("id"));
    if (!type.has_value() || !protocolTypeOf || !allocateEntityId) {
        return nullptr;
    }
    const std::optional<std::int32_t> protocolType = protocolTypeOf(*type);
    if (!protocolType.has_value()) {
        return nullptr;
    }
    std::unique_ptr<ServerEntity> decoded = decode(tag, allocateEntityId(*type), *protocolType);
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
                                      const AllocateEntityIdFor& allocateEntityId,
                                      const ProtocolTypeOf& protocolTypeOf) {
    if (!protocolTypeOf || !allocateEntityId) {
        return nullptr;
    }
    const std::optional<std::int32_t> protocolType = protocolTypeOf(descriptor.type);
    if (!protocolType.has_value()) {
        return nullptr;
    }
    std::unique_ptr<ServerEntity> decoded =
        decodeGenerated(descriptor, allocateEntityId(descriptor.type), *protocolType);
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

/// parity: `uuidFromIntArray`.
[[nodiscard]] std::optional<wire::common::Uuid> uuidIn(const NbtCompound& tag, std::string_view key) {
    if (const std::span<const std::int32_t> parts = tag.getIntArray(key); parts.size() == 4) {
        const auto word = [parts](std::size_t at) {
            return static_cast<std::uint64_t>(static_cast<std::uint32_t>(parts[at]));
        };
        return wire::common::Uuid{(word(0) << 32) | word(1), (word(2) << 32) | word(3)};
    }
    return wire::common::Uuid::parse(tag.getString(key));
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
    const std::optional<Uuid> uuid = uuidIn(tag, "UUID");
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
    // parity: a body saved mid-count resumes mid-count; `isDeadOrDying` puts it back on the clock.
    entity->deathTime = numericInt(tag, "DeathTime");
    entity->dying = entity->deathTime > 0;
    entity->fireTicks = numericInt(tag, "Fire");
    entity->lastHurtByPlayer = uuidIn(tag, "last_hurt_by_player");
    entity->lastHurtByPlayerTime = numericInt(tag, "last_hurt_by_player_memory_time");
    entity->noGravity = numericBoolean(tag, "NoGravity");
    const bool arrow = isArrow(*type);
    if (const NbtCompound* held = tag.getCompound(arrow ? "item" : "Item"); held != nullptr) {
        const std::expected<Identifier, wire::common::IdentifierError> item = Identifier::parse(held->getString("id"));
        if (item.has_value()) {
            entity->heldItem = std::string{item->path()};
        }
        if (const NbtCompound* components = held->getCompound("components");
            (arrow || type->path().ends_with("_potion")) && components != nullptr) {
            entity->heldItemComponents = *components;
        }
    }
    if (arrow) {
        const std::int8_t pickup = tag.getByte("pickup");
        entity->arrowPickup = pickup >= 0 && pickup <= 2 ? static_cast<ServerEntity::ArrowPickup>(pickup)
                                                         : ServerEntity::ArrowPickup::Disallowed;
        entity->pierceLevel = tag.getByte("PierceLevel");
        entity->arrowLife = tag.getShort("life");
        entity->onGround = numericBoolean(tag, "inGround");
        entity->tridentDealtDamage = numericBoolean(tag, "DealtDamage");
    }
    if (type->path() == "ravager") {
        entity->ravagerAttackTicks = numericInt(tag, "AttackTick");
        entity->ravagerStunTicks = numericInt(tag, "StunTick");
        entity->ravagerRoarTicks = numericInt(tag, "RoarTick");
    }
    const std::string_view path = type->path();
    entity->age = ageableMobs::usesBooleanBabyFlag(path) && numericBoolean(tag, "IsBaby") ? ageableMobs::kBabyStartAge
                                                                                          : numericInt(tag, "Age");
    if (tag.contains("fuse")) {
        entity->fuse = numericInt(tag, "fuse");
    }
    if (minecarts::isMinecart(*type)) {
        minecarts::load(*entity, tag);
    }
    if (tag.contains("Air")) {
        entity->airSupply = static_cast<std::int32_t>(tag.getShort("Air"));
    }
    entity->cubeSize =
        cubeMobs::isCube(*type) ? std::clamp(numericInt(tag, "Size") + 1, 1, 127) : cubeMobs::kDefaultSize;
    entity->persistent = numericBoolean(tag, "PersistenceRequired");
    entity->noAi = numericBoolean(tag, "NoAI");
    entity->canPickUpLoot = tag.getBoolean("CanPickUpLoot");
    entity->loveTicks = numericInt(tag, "InLove");
    if (path == "zombie" || path == "husk") {
        entity->conversionClock = numericInt(tag, "InWaterTime");
        entity->conversionCountdown =
            tag.contains("DrownedConversionTime") ? numericInt(tag, "DrownedConversionTime") : -1;
    }
    if (path == "zombie_villager") {
        entity->conversionCountdown = tag.contains("ConversionTime") ? numericInt(tag, "ConversionTime") : -1;
    }
    if (path == "skeleton") {
        entity->conversionCountdown = tag.contains("StrayConversionTime") ? numericInt(tag, "StrayConversionTime") : -1;
    }
    if (path == "piglin" || path == "piglin_brute" || path == "hoglin") {
        entity->immuneToZombification = numericBoolean(tag, "IsImmuneToZombification");
        entity->conversionClock = numericInt(tag, "TimeInOverworld");
    }
    if (path == "tadpole") {
        entity->conversionClock = numericInt(tag, "Age");
    }
    entity->powered = path == "creeper" && numericBoolean(tag, "powered");
    if (tag.contains("Paper.canWander") && !numericBoolean(tag, "Paper.canWander")) {
        mobMisc::of(*entity).cubeWanders = false;
    }
    if (path == "creeper" && numericBoolean(tag, "ignited")) {
        mobMisc::of(*entity).creeperIgnited = true;
    }
    mobStatusEffects::decode(*entity, tag);
    if (path == "dolphin") {
        entity->gotFish = numericBoolean(tag, "GotFish");
        entity->moistness = tag.contains("Moistness") ? numericInt(tag, "Moistness") : 2400;
    }
    if (path == "pufferfish") {
        // parity: `Pufferfish.readAdditionalSaveData`, capped at the fully puffed state.
        entity->puffState = std::min(numericInt(tag, "PuffState"), 2);
    }
    if (path == "wither") {
        entity->wither = std::make_shared<witherBoss::WitherState>();
        entity->wither->invulnerableTicks = numericInt(tag, witherBoss::kInvulnerableKey);
        entity->wither->random = random::RandomSource::legacy(entity->entityId());
    }
    entity->dangerousSkull = path == "wither_skull" && numericBoolean(tag, "dangerous");
    if (path == "firework_rocket") {
        fireworkRockets::read(tag, *entity);
    }
    entity->breedCooldown = std::max(entity->age, 0);
    if (patrols(path)) {
        if (const std::span<const std::int32_t> target = tag.getIntArray("patrol_target"); target.size() >= 3) {
            entity->patrolTarget = std::array<std::int32_t, 3>{target[0], target[1], target[2]};
        }
        entity->patrolLeader = numericBoolean(tag, "PatrolLeader");
        entity->patrolling = numericBoolean(tag, "Patrolling");
        entity->raidWave = numericInt(tag, "Wave");
        entity->canJoinRaid = numericBoolean(tag, "CanJoinRaid");
        if (tag.contains("RaidId")) {
            entity->raidId = numericInt(tag, "RaidId");
        }
    }
    if ((path == "wandering_trader" || path == "trader_llama") && tag.contains("DespawnDelay")) {
        entity->despawnDelay = tag.getInt("DespawnDelay");
    }
    if (path == "wandering_trader") {
        if (const std::span<const std::int32_t> target = tag.getIntArray("wander_target"); target.size() >= 3) {
            entity->wanderTarget = std::array<std::int32_t, 3>{target[0], target[1], target[2]};
        }
    }
    if (path == "wandering_trader") {
        if (numericBoolean(tag, "Paper.CanDrinkPotionDisabled")) {
            mobMisc::of(*entity).traderNoPotion = true;
        }
        if (numericBoolean(tag, "Paper.CanDrinkMilkDisabled")) {
            mobMisc::of(*entity).traderNoMilk = true;
        }
    }
    // parity: `Turtle.readAdditionalSaveData`, the home falling back to where it stands.
    if (path == "turtle") {
        mobMisc::State& held = mobMisc::of(*entity);
        if (const std::span<const std::int32_t> home = tag.getIntArray("home_pos"); home.size() >= 3) {
            held.turtleHome = std::array<std::int32_t, 3>{home[0], home[1], home[2]};
        } else {
            held.turtleHome = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(entity->x)),
                                                          static_cast<std::int32_t>(std::floor(entity->y)),
                                                          static_cast<std::int32_t>(std::floor(entity->z))};
        }
        held.turtleHasEgg = numericBoolean(tag, "has_egg");
    }
    if (path == "skeleton_horse") {
        entity->skeletonTrap = numericBoolean(tag, "SkeletonTrap");
        entity->skeletonTrapTime = numericInt(tag, "SkeletonTrapTime");
    }
    if (horses::isHorse(entity->type())) {
        entity->tamedHorse = numericBoolean(tag, "Tame");
    }
    if (const std::optional<Uuid> leash = uuidIn(compoundOf(tag, "leash"), "UUID");
        leash.has_value() && !leash->isNil()) {
        entity->leashHolderUuid = leash->toString();
    }
    if (const std::span<const std::int32_t> knot = tag.getIntArray("leash"); knot.size() == 3) {
        entity->leashKnotAt = std::array<std::int32_t, 3>{knot[0], knot[1], knot[2]};
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
        villager::villagerNbt::read(tag, *entity);
        villagers::readOffers(tag, *entity);
    }
    if (const std::string_view owner = tag.getString("Owner"); !owner.empty()) {
        entity->owner = std::string{owner};
    }
    if (projectiles::kindOf(*type) != nullptr) {
        entity->ownerUuid = uuidIn(tag, "Owner");
        entity->leftOwner = tag.getBoolean("LeftOwner");
    }
    entity->sitting = numericBoolean(tag, "Sitting");
    if (const std::string_view customName = tag.getString("CustomName"); !customName.empty()) {
        entity->customName = std::string{customName};
    }
    if (const std::string_view table = tag.getString("DeathLootTable"); !table.empty()) {
        entity->deathLootTable = std::string{table};
    }
    entity->deathLootTableSeed = tag.getLong("DeathLootTableSeed");
    if (const NbtCompound* pluginData = tag.getCompound("PublicBukkitValues"); pluginData != nullptr) {
        entity->pluginData = *pluginData;
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

std::vector<std::unique_ptr<ServerEntity>>
decodeTree(const NbtCompound& tag, const AllocateEntityIdFor& allocateEntityId, const ProtocolTypeOf& protocolTypeOf) {
    std::vector<std::unique_ptr<ServerEntity>> entities;
    (void)decodeTreeNode(tag, nullptr, entities, allocateEntityId, protocolTypeOf);
    return entities;
}

std::vector<std::unique_ptr<ServerEntity>> decodeTree(const NbtCompound& tag, const AllocateEntityId& allocateEntityId,
                                                      const ProtocolTypeOf& protocolTypeOf) {
    if (!allocateEntityId) {
        return {};
    }
    return decodeTree(tag, AllocateEntityIdFor{[&allocateEntityId](const Identifier&) { return allocateEntityId(); }},
                      protocolTypeOf);
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
                                                               const AllocateEntityIdFor& allocateEntityId,
                                                               const ProtocolTypeOf& protocolTypeOf) {
    std::vector<std::unique_ptr<ServerEntity>> entities;
    (void)decodeGeneratedTreeNode(descriptor, nullptr, entities, allocateEntityId, protocolTypeOf);
    return entities;
}

std::vector<std::unique_ptr<ServerEntity>> decodeGeneratedTree(const server::world::ChunkGeneratedEntity& descriptor,
                                                               const AllocateEntityId& allocateEntityId,
                                                               const ProtocolTypeOf& protocolTypeOf) {
    if (!allocateEntityId) {
        return {};
    }
    return decodeGeneratedTree(
        descriptor, AllocateEntityIdFor{[&allocateEntityId](const Identifier&) { return allocateEntityId(); }},
        protocolTypeOf);
}

} // namespace basalt::server::runtime::entity::entityNbt
