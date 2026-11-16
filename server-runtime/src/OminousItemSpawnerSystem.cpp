#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/OminousItemSpawnerSystem.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::common::Identifier;

constexpr std::int32_t kDelayMin = 60;
constexpr std::int32_t kDelayMax = 120;
constexpr std::int64_t kTicksBeforeAboutToSpawn = 36;
constexpr std::int32_t kItemSpawnedEvent = 3021;
constexpr double kLevelEventRange = 64.0;
constexpr double kSoundRange = 16.0;
constexpr const char* kItemKey = "item";
constexpr const char* kDelayKey = "spawn_item_after_ticks";

[[nodiscard]] std::array<std::int32_t, 3> blockOf(const ServerEntity& entity) {
    return {static_cast<std::int32_t>(std::floor(entity.x)), static_cast<std::int32_t>(std::floor(entity.y)),
            static_cast<std::int32_t>(std::floor(entity.z))};
}

[[nodiscard]] std::int64_t delayOf(const ServerEntity& spawner) {
    if (!spawner.sourceNbt.has_value()) {
        return 0;
    }
    const wire::nbt::NbtCompound& tag = *spawner.sourceNbt;
    if (tag.contains(kDelayKey)) {
        const std::int64_t asLong = tag.getLong(kDelayKey, -1);
        return asLong >= 0 ? asLong : tag.getInt(kDelayKey, tag.getShort(kDelayKey, tag.getByte(kDelayKey)));
    }
    return 0;
}

[[nodiscard]] std::optional<server::world::loot::ItemStack> itemOf(const ServerEntity& spawner) {
    if (!spawner.sourceNbt.has_value()) {
        return std::nullopt;
    }
    const wire::nbt::NbtCompound* tag = spawner.sourceNbt->getCompound(kItemKey);
    if (tag == nullptr) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id =
        Identifier::parse(std::string{tag->getString("id")});
    const std::int32_t count = tag->getInt("count", 1);
    if (!id.has_value() || count <= 0) {
        return std::nullopt;
    }
    server::world::loot::ItemStack stack{.item = *id, .count = count};
    if (const wire::nbt::NbtCompound* components = tag->getCompound("components"); components != nullptr) {
        stack.components = *components;
    }
    return stack;
}

} // namespace

OminousItemSpawnerSystem::OminousItemSpawnerSystem(player::PlayerRegistry& players, Options options)
    : players_(&players), options_(std::move(options)) {}

bool OminousItemSpawnerSystem::isSpawner(const ServerEntity& entity) {
    return (entity.typeTraits() & entity::traits::kOminousItemSpawner) != 0;
}

ServerEntity* OminousItemSpawnerSystem::spawn(world::LoadedWorld& world, const server::world::loot::ItemStack& stack,
                                              const double x, const double y, const double z,
                                              random::RandomSource& random) {
    const registry::EntityTypeEntry* type = registry::entityTypes::of(Identifier::vanilla("ominous_item_spawner"));
    if (type == nullptr) {
        return nullptr;
    }
    auto spawner = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), type->id,
                                                  type->protocolId, x, y, z);
    spawner->persistent = true;
    wire::nbt::NbtCompound item;
    item.putString("id", stack.item.toString());
    item.putInt("count", stack.count);
    if (!stack.components.empty()) {
        item.set("components", wire::nbt::NbtTag::ofCompound(stack.components));
    }
    spawner->sourceNbt.emplace();
    spawner->sourceNbt->set(kItemKey, wire::nbt::NbtTag::ofCompound(std::move(item)));
    spawner->sourceNbt->putLong(kDelayKey, random.nextIntBetweenInclusive(kDelayMin, kDelayMax));
    return &world.state().spawnEntity(std::move(spawner));
}

void OminousItemSpawnerSystem::tick(world::LoadedWorld& world, ServerEntity& spawner, random::RandomSource& random) {
    ++spawner.tickCount;
    const std::int64_t after = delayOf(spawner);
    if (spawner.tickCount == after - kTicksBeforeAboutToSpawn) {
        const std::array<std::int32_t, 3> at = blockOf(spawner);
        const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
        for (const player::PlayerSession* player : players_->snapshot()) {
            const double dx = at[0] + 0.5 - player->x;
            const double dy = at[1] + 0.5 - player->y;
            const double dz = at[2] + 0.5 - player->z;
            if (player->worldName == world.name() && (dx * dx) + (dy * dy) + (dz * dz) < kSoundRange * kSoundRange) {
                player::playSoundFor(*player, "block.trial_spawner.about_to_spawn_item",
                                     wire::protocol::play::SoundSource::Neutral, at[0] + 0.5, at[1] + 0.5, at[2] + 0.5,
                                     1.0F, 1.0F, seed);
            }
        }
    }
    if (spawner.tickCount >= after) {
        if (const std::optional<server::world::loot::ItemStack> stack = itemOf(spawner); stack.has_value()) {
            spawnItem(world, spawner, *stack, random);
        }
        spawner.removed = true;
    }
}

void OminousItemSpawnerSystem::spawnItem(world::LoadedWorld& world, const ServerEntity& spawner,
                                         const server::world::loot::ItemStack& stack, random::RandomSource& random) {
    const std::array<std::int32_t, 3> at = blockOf(spawner);
    const std::optional<entity::projectiles::DispenseShot> shot = entity::projectiles::dispensed(stack.item);
    const registry::EntityTypeEntry* type = shot.has_value() ? registry::entityTypes::of(shot->kind->type) : nullptr;
    wire::common::Uuid placed;
    if (type != nullptr) {
        if (shot->overrideEvent.has_value()) {
            levelEvent(world, *shot->overrideEvent, at, 0);
        }
        ServerEntity& launched = entity::projectiles::spawn(world.state(), *shot->kind, players_->allocateEntityId(),
                                                            type->protocolId, spawner.x, spawner.y, spawner.z, 0.0,
                                                            -1.0, 0.0, shot->power, shot->uncertainty, random);
        entity::projectiles::arm(launched, *shot, stack);
        launched.ownerId = spawner.entityId();
        launched.ownerUuid = spawner.uuid();
        placed = launched.uuid();
    } else {
        world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                                   .uuid = wire::common::Uuid::random(),
                                   .item = stack.item,
                                   .count = stack.count,
                                   .x = spawner.x,
                                   .y = spawner.y,
                                   .z = spawner.z};
        dropped.components = stack.components;
        dropped.velocityX = (random.nextDouble() * 0.2) - 0.1;
        dropped.velocityY = 0.2;
        dropped.velocityZ = (random.nextDouble() * 0.2) - 0.1;
        placed = dropped.uuid;
        static_cast<void>(world.state().spawnItem(std::move(dropped)));
    }
    levelEvent(world, kItemSpawnedEvent, at, 1);
    world::vibrations::emit(world.state(), "entity_place", spawner.x, spawner.y, spawner.z, world.gameTime(),
                            {.entity = placed});
}

void OminousItemSpawnerSystem::levelEvent(world::LoadedWorld& world, const std::int32_t type,
                                          const std::array<std::int32_t, 3>& at, const std::int32_t data) const {
    for (player::PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || !player->connection().sendLevelEvent) {
            continue;
        }
        const double dx = at[0] + 0.5 - player->x;
        const double dy = at[1] + 0.5 - player->y;
        const double dz = at[2] + 0.5 - player->z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < kLevelEventRange * kLevelEventRange) {
            player->connection().sendLevelEvent(type, at[0], at[1], at[2], data);
        }
    }
}

} // namespace basalt::server::runtime
