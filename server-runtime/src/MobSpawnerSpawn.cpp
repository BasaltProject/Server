#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobSpawnerSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/EquipmentTables.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobSpawnChecks.hpp>
#include <basalt/server/runtime/world/NaturalSpawnObstruction.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::Aabb;
using entity::ServerEntity;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

/// parity: `LevelEvent.PARTICLES_MOBBLOCK_SPAWN`, the smoke and flames round the cage.
constexpr std::int32_t kSpawnParticlesEvent = 2004;
/// parity: the entity event `Mob.spawnAnim` broadcasts.
constexpr std::int8_t kSpawnAnimEvent = 20;
/// parity: `ServerLevel.levelEvent`, heard by the players within sixty-four blocks.
constexpr double kLevelEventRange = 64.0;
constexpr float kFullTurn = 360.0F;
constexpr std::size_t kPosComponents = 3;

struct Loaded {
    std::vector<std::unique_ptr<ServerEntity>> entities;
    ServerEntity* main = nullptr;
    std::vector<std::pair<ServerEntity*, std::int32_t>> externalMounts;
};

/// parity: `EntityType.by(input)`, the `id` of a type this build knows.
[[nodiscard]] std::optional<Identifier> typeOf(const NbtCompound& tag) {
    const NbtTag* id = tag.find("id");
    const auto* text = id == nullptr ? nullptr : id->get<std::string>();
    if (text == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(*text);
    if (!parsed.has_value() || server::registry::entityTypes::of(*parsed) == nullptr) {
        return std::nullopt;
    }
    return *parsed;
}

/// parity: `input.read("Pos", Vec3.CODEC)`: exactly three numbers.
[[nodiscard]] std::optional<std::array<double, 3>> savedPosition(const NbtCompound& tag) {
    const NbtList* list = tag.getList("Pos");
    if (list == nullptr || list->size() != kPosComponents) {
        return std::nullopt;
    }
    std::array<double, 3> position{};
    for (std::size_t axis = 0; axis < kPosComponents; ++axis) {
        const std::optional<double> value = world::baseSpawnerData::doubleValue((*list)[axis]);
        if (!value.has_value()) {
            return std::nullopt;
        }
        position.at(axis) = *value;
    }
    return position;
}

/// parity: `EntityTypeTest.forExactClass`.
[[nodiscard]] std::string classOf(const Identifier& type) {
    const std::string& path = type.path();
    if (type.namespaceName() == Identifier::kVanillaNamespace && path.ends_with("_chest_boat")) {
        return "chest_boat";
    }
    if (type.namespaceName() == Identifier::kVanillaNamespace && path.ends_with("_boat")) {
        return "boat";
    }
    return type.toString();
}

/// parity: `MobCategory.isFriendly`, which only the monsters are not.
[[nodiscard]] bool isFriendly(const Identifier& type) {
    const std::optional<entity::MobCategory> category =
        entity::mobCategoryByName(server::registry::entityTypeShapes::categoryOf(type));
    return !category.has_value() || entity::mobCategoryFacts(*category).friendly;
}

[[nodiscard]] Aabb boxOf(const ServerEntity& entity) {
    return Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
}

[[nodiscard]] std::optional<std::int32_t> protocolTypeOf(const Identifier& type) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(type);
    return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
}

/// parity: `startRiding` onto a chicken marks it as a jockey's mount.
void mount(ServerEntity& rider, ServerEntity& vehicle) {
    rider.vehicleId = vehicle.entityId();
    vehicle.passengerIds.push_back(rider.entityId());
    if (vehicle.type().path() == "chicken") {
        if (!vehicle.sourceNbt.has_value()) {
            vehicle.sourceNbt = NbtCompound{};
        }
        vehicle.sourceNbt->putBoolean("IsChickenJockey", true);
    }
}

[[nodiscard]] Loaded decodeForest(const world::MobSpawnForest& forest,
                                  const entity::entityNbt::AllocateEntityIdFor& allocate) {
    Loaded loaded;
    const auto decodeNode = [&](auto&& self, const server::world::ChunkGeneratedEntity& descriptor,
                                ServerEntity* vehicle) -> void {
        const std::optional<std::int32_t> protocolType = protocolTypeOf(descriptor.type);
        if (!protocolType.has_value()) {
            return;
        }
        std::unique_ptr<ServerEntity> decoded =
            entity::entityNbt::decodeGenerated(descriptor, allocate(descriptor.type), *protocolType);
        if (decoded == nullptr) {
            return;
        }
        ServerEntity* node = decoded.get();
        if (vehicle != nullptr) {
            mount(*node, *vehicle);
        } else {
            for (const world::MobExternalVehicleLink& link : forest.externalVehicleLinks) {
                if (link.passenger == descriptor) {
                    loaded.externalMounts.emplace_back(node, link.vehicleId);
                    break;
                }
            }
        }
        if (loaded.main == nullptr && descriptor == forest.main) {
            loaded.main = node;
        }
        loaded.entities.push_back(std::move(decoded));
        for (const server::world::ChunkGeneratedEntity& passenger : descriptor.passengers) {
            self(self, passenger, node);
        }
    };
    for (const server::world::ChunkGeneratedEntity& root : forest.roots) {
        decodeNode(decodeNode, root, nullptr);
    }
    return loaded;
}

} // namespace

MobSpawnerSystem::Attempt MobSpawnerSystem::attempt(Turn& turn, const world::SpawnData& spawn) {
    world::LoadedWorld& world = turn.world;
    random::RandomSource& random = turn.random;
    const NbtCompound& tag = spawn.entity;
    const std::optional<Identifier> type = typeOf(tag);
    if (!type.has_value()) {
        return Attempt::Abort;
    }
    // parity: `Pos`.
    std::array<double, 3> at{};
    if (const std::optional<std::array<double, 3>> saved = savedPosition(tag); saved.has_value()) {
        at = *saved;
    } else {
        const double range = static_cast<double>(turn.state.spawnRange);
        const double x1 = random.nextDouble();
        const double x2 = random.nextDouble();
        at[0] = turn.pos[0] + ((x1 - x2) * range) + 0.5;
        at[1] = static_cast<double>(turn.pos[1] + random.nextInt(3) - 1);
        const double z1 = random.nextDouble();
        const double z2 = random.nextDouble();
        at[2] = turn.pos[2] + ((z1 - z2) * range) + 0.5;
    }
    const auto [x, y, z] = at;
    if (!world::mobSpawnChecks::noCollision(world, world::naturalSpawnObstruction::spawnBox(*type, x, y, z))) {
        return Attempt::Skipped;
    }
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    if (spawn.customSpawnRules.has_value()) {
        if (!isFriendly(*type) && world.difficulty() == "peaceful") {
            return Attempt::Skipped;
        }
        // parity: `CustomSpawnRules.isValidPosition`.
        const world::CustomSpawnRules& rules = *spawn.customSpawnRules;
        if (!rules.blockLight.contains(world::spawnPlacements::blockBrightness(world, blockX, blockY, blockZ)) ||
            !rules.skyLight.contains(world::spawnPlacements::effectiveSkyBrightness(world, blockX, blockY, blockZ))) {
            return Attempt::Skipped;
        }
    } else if (!world::spawnPlacements::checkSpawnRules(world, *type, world::MobSpawnReason::Spawner, blockX, blockY,
                                                        blockZ, random, options_.facts)) {
        return Attempt::Skipped;
    }

    const std::array<std::int32_t, 3>& pos = turn.pos;
    const auto spawnRange = static_cast<double>(turn.state.spawnRange);
    const Aabb cage = Aabb{.minX = static_cast<double>(pos[0]),
                           .minY = static_cast<double>(pos[1]),
                           .minZ = static_cast<double>(pos[2]),
                           .maxX = static_cast<double>(pos[0] + 1),
                           .maxY = static_cast<double>(pos[1] + 1),
                           .maxZ = static_cast<double>(pos[2] + 1)}
                          .inflate(spawnRange);

    // An item on the floor is not a `ServerEntity` here, so it loads and counts its own way.
    if (type->namespaceName() == Identifier::kVanillaNamespace && type->path() == "item") {
        std::optional<world::DroppedItem> item =
            world::droppedItemNbt::decode(tag, [this] { return players_->allocateEntityId(); });
        if (!item.has_value()) {
            return Attempt::Abort;
        }
        const double width = server::registry::entityTypeShapes::widthOf(*type);
        const double height = server::registry::entityTypeShapes::heightOf(*type);
        std::int32_t nearby = 0;
        for (const world::DroppedItem& other : world.state().items()) {
            nearby += Aabb::of(other.x, other.y, other.z, width, height).intersects(cage) ? 1 : 0;
        }
        if (nearby >= turn.state.maxNearbyEntities) {
            return Attempt::Abort;
        }
        static_cast<void>(random.nextFloat());
        item->x = x;
        item->y = y;
        item->z = z;
        const wire::common::Uuid uuid = item->uuid;
        static_cast<void>(world.state().spawnItem(std::move(*item)));
        broadcastSpawned(world, pos, blockX, blockY, blockZ, uuid, std::nullopt);
        return Attempt::Spawned;
    }

    const entity::entityNbt::AllocateEntityIdFor allocate = [this](const Identifier& kind) {
        return players_->allocateEntityIds(entity::enderDragon::idSpan(kind));
    };
    // parity: `EntityType.loadEntityRecursive`, every rider snapped to the same spot.
    Loaded loaded;
    loaded.entities = entity::entityNbt::decodeTree(tag, allocate, protocolTypeOf);
    if (loaded.entities.empty()) {
        return Attempt::Abort;
    }
    loaded.main = loaded.entities.front().get();
    for (const std::unique_ptr<ServerEntity>& entity : loaded.entities) {
        entity->x = x;
        entity->y = y;
        entity->z = z;
    }
    const std::string kind = classOf(*type);
    std::int32_t nearby = 0;
    for (const ServerEntity* other : world.state().entities()) {
        nearby += !other->removed && classOf(other->type()) == kind && boxOf(*other).intersects(cage) ? 1 : 0;
    }
    if (nearby >= turn.state.maxNearbyEntities) {
        return Attempt::Abort;
    }
    const float yaw = random.nextFloat() * kFullTurn;
    loaded.main->yaw = yaw;
    loaded.main->pitch = 0.0F;

    const bool isMob = world::mobSpawnChecks::isMob(*type);
    if (isMob) {
        const std::vector<Aabb> players = playerBoxes(world);
        if ((!spawn.customSpawnRules.has_value() &&
             !world::mobSpawnChecks::checkSpawnRules(world, *type, blockX, blockY, blockZ, options_.facts)) ||
            !world::mobSpawnChecks::checkSpawnObstruction(world, *type, boxOf(*loaded.main), x, y, z, players,
                                                          options_.facts)) {
            return Attempt::Skipped;
        }
        // parity: only a bare `{id}` is finalized; anything configured spawns as it was written.
        const NbtTag* id = tag.find("id");
        if (tag.size() == 1 && id != nullptr && id->get<std::string>() != nullptr) {
            loaded = decodeForest(spawning_->finalizeForSpawner(world, *type, x, y, z, yaw, random), allocate);
            if (loaded.main == nullptr) {
                return Attempt::Abort;
            }
        }
        if (spawn.equipment.has_value()) {
            if (!loaded.main->sourceNbt.has_value()) {
                loaded.main->sourceNbt = NbtCompound{};
            }
            equip(world, *loaded.main->sourceNbt, *spawn.equipment, x, y, z, random);
        }
    }

    // parity: `tryAddFreshEntityWithPassengers` refuses when any uuid of the tree is already loaded.
    for (const std::unique_ptr<ServerEntity>& entity : loaded.entities) {
        for (const ServerEntity* other : world.state().entities()) {
            if (other->uuid() == entity->uuid()) {
                return Attempt::Abort;
            }
        }
        for (const player::PlayerSession* player : playersIn(world)) {
            if (player->uuid() == entity->uuid()) {
                return Attempt::Abort;
            }
        }
    }
    const wire::common::Uuid mainUuid = loaded.main->uuid();
    const std::int32_t mainId = loaded.main->entityId();
    for (const auto& [rider, vehicleId] : loaded.externalMounts) {
        if (ServerEntity* vehicle = world.state().entity(vehicleId); vehicle != nullptr) {
            mount(*rider, *vehicle);
        }
    }
    for (std::unique_ptr<ServerEntity>& entity : loaded.entities) {
        static_cast<void>(world.state().spawnEntity(std::move(entity)));
    }
    broadcastSpawned(world, pos, blockX, blockY, blockZ, mainUuid,
                     isMob ? std::optional<std::int32_t>{mainId} : std::nullopt);
    return Attempt::Spawned;
}

void MobSpawnerSystem::broadcastSpawned(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos,
                                        std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                        const wire::common::Uuid& source,
                                        std::optional<std::int32_t> animatedMob) const {
    const std::vector<player::PlayerSession*> players = playersIn(world);
    // parity: `level.levelEvent(2004, pos, 0)`, measured from the cage's corner.
    for (player::PlayerSession* player : players) {
        const double dx = pos[0] - player->x;
        const double dy = pos[1] - player->y;
        const double dz = pos[2] - player->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange &&
            player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(kSpawnParticlesEvent, pos[0], pos[1], pos[2], 0);
        }
    }
    // parity: `level.gameEvent(entity, ENTITY_PLACE, spawnBlockPos)`.
    world::vibrations::emit(world.state(), "entity_place", blockX + 0.5, blockY + 0.5, blockZ + 0.5, world.gameTime(),
                            {.entity = source});
    if (!animatedMob.has_value()) {
        return;
    }
    // parity: `Mob.spawnAnim`, which the server says as an entity event.
    const wire::protocol::play::EntityEventPacket packet{.entityId = *animatedMob, .eventId = kSpawnAnimEvent};
    for (player::PlayerSession* player : players) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void MobSpawnerSystem::equip(world::LoadedWorld& world, NbtCompound& mob, const world::EquipmentTable& table, double x,
                             double y, double z, random::RandomSource& random) const {
    if (options_.lootTables == nullptr) {
        return;
    }
    const server::world::loot::LootTable* loot = options_.lootTables->byId(table.lootTable);
    if (loot == nullptr) {
        return;
    }
    // parity: `Mob.createEquipmentParams`, the mob as `THIS_ENTITY` at its own position.
    server::world::loot::LootContext context{.hasEntity = true};
    const std::optional<Identifier> type = typeOf(mob);
    if (type.has_value()) {
        context.thisEntity = server::world::loot::EntityFacts{.type = *type, .baby = mob.getBoolean("IsBaby")};
    }
    options_.lootTables->prepareAt(context, world, x, y, z);
    // parity: a seed of zero rolls on the table's own sequence, or on the level's stream.
    random::RandomSource& roll =
        loot->randomSequence.has_value() ? world.randomSequences().of(*loot->randomSequence) : random;
    world::equipmentTables::equip(mob, table, loot->generate(roll, 0.0F, std::nullopt, context));
}

} // namespace basalt::server::runtime
