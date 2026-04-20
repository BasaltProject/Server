#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/Parrots.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::SoundSource;

/// parity: `imitateNearbyMobs`, the mobs within twenty of the imitator's box.
constexpr double kImitateRange = 20.0;
/// parity: the player box a shoulder parrot is heard from.
constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;
/// parity: `handleShoulderEntities`, the fall that throws a parrot off.
constexpr double kShoulderFall = 0.5;
/// parity: `removeEntitiesOnShoulder`.
constexpr std::int64_t kShoulderSettleTicks = 20;
/// parity: `respawnEntityOnShoulder`, put back 0.7 above the player's feet.
constexpr double kShoulderHeight = static_cast<double>(0.7F);

void soundAt(const std::vector<PlayerSession*>& watchers, std::string_view sound, SoundSource source, double x,
             double y, double z, float volume, float pitch, std::int64_t seed) {
    for (const PlayerSession* watcher : watchers) {
        player::playSoundFor(*watcher, sound, source, x, y, z, volume, pitch, seed);
    }
}

[[nodiscard]] bool feetInWater(world::LoadedWorld& world, const PlayerSession& player) {
    const std::optional<std::int32_t> state = world.state().blockState(static_cast<std::int32_t>(std::floor(player.x)),
                                                                       static_cast<std::int32_t>(std::floor(player.y)),
                                                                       static_cast<std::int32_t>(std::floor(player.z)));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

[[nodiscard]] bool silent(const wire::nbt::NbtCompound& tag) { return tag.getBoolean("Silent"); }

} // namespace

bool MobActionSystem::imitateNearbyMobs(world::LoadedWorld& world, double x, double y, double z, double width,
                                        double height, SoundSource source, random::RandomSource& random) {
    if (random.nextInt(2) != 0) {
        return false;
    }
    const double half = (width / 2.0) + kImitateRange;
    std::vector<const ServerEntity*> mobs;
    for (const ServerEntity* mob : world.state().entities()) {
        if (mob->removed || !entity::parrots::imitationOf(mob->type().path()).has_value() ||
            mob->x + (mob->width() / 2.0) < x - half || mob->x - (mob->width() / 2.0) > x + half ||
            mob->y + mob->height() < y - kImitateRange || mob->y > y + height + kImitateRange ||
            mob->z + (mob->width() / 2.0) < z - half || mob->z - (mob->width() / 2.0) > z + half) {
            continue;
        }
        mobs.push_back(mob);
    }
    if (mobs.empty()) {
        return false;
    }
    const ServerEntity& mob = *mobs[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(mobs.size())))];
    if (mob.sourceNbt.has_value() && silent(*mob.sourceNbt)) {
        return false;
    }
    const std::string_view imitation = *entity::parrots::imitationOf(mob.type().path());
    const std::string sound = imitation.empty() ? std::string{"minecraft:intentionally_empty"}
                                                : "minecraft:entity.parrot.imitate." + std::string{imitation};
    const float pitch = entity::parrots::pitch(random);
    soundAt(watchersOf(world), sound, source, x, y, z, 0.7F, pitch, random.nextLong());
    return true;
}

void MobActionSystem::tickParrot(world::LoadedWorld& world, ServerEntity& parrot) {
    // parity: `Parrot.aiStep` rolls the level's own random.
    // and not silenced.
    random::RandomSource random = freshRandom();
    if (random.nextInt(400) != 0 || parrot.dying || parrot.health <= 0.0 ||
        (parrot.sourceNbt.has_value() && silent(*parrot.sourceNbt))) {
        return;
    }
    static_cast<void>(imitateNearbyMobs(world, parrot.x, parrot.y, parrot.z, parrot.width(), parrot.height(),
                                        SoundSource::Neutral, random));
}

bool MobActionSystem::sitOnShoulder(ServerEntity& parrot, std::int32_t playerId) {
    world::LoadedWorld* world = worldOf(parrot);
    if (world == nullptr || parrot.removed) {
        return false;
    }
    PlayerSession* player = nullptr;
    for (PlayerSession* candidate : watchersOf(*world)) {
        if (candidate->entityId() == playerId) {
            player = candidate;
        }
    }
    // parity: `ServerPlayer.setEntityOnShoulder`.
    if (player == nullptr || player->vehicleId.has_value() || !player->onGround || feetInWater(*world, *player)) {
        return false;
    }
    std::optional<wire::nbt::NbtCompound>* shoulder = !player->shoulderLeft.has_value()    ? &player->shoulderLeft
                                                      : !player->shoulderRight.has_value() ? &player->shoulderRight
                                                                                           : nullptr;
    if (shoulder == nullptr) {
        return false;
    }
    wire::nbt::NbtCompound tag = entity::entityNbt::encode(parrot);
    tag.putString("id", parrot.type().toString());
    *shoulder = std::move(tag);
    player->timeEntitySatOnShoulder = world->gameTime();
    // parity: `discard`, off the ground without dying.
    parrot.removed = true;
    return true;
}

void MobActionSystem::respawnShoulder(world::LoadedWorld& world, PlayerSession& player,
                                      const wire::nbt::NbtCompound& tag) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> type =
        wire::common::Identifier::parse(std::string{tag.getString("id")});
    const server::registry::EntityTypeEntry* entry =
        type.has_value() ? server::registry::entityTypes::of(*type) : nullptr;
    if (entry == nullptr) {
        return;
    }
    std::unique_ptr<ServerEntity> created =
        entity::entityNbt::decode(tag, players_->allocateEntityId(), entry->protocolId);
    if (created == nullptr) {
        return;
    }
    created->sourceNbt = tag;
    // parity: a tamed one is given back to whoever carried it.
    if (created->owner.has_value() || tag.contains("Owner")) {
        created->owner = player.uuid().toString();
    }
    created->x = player.x;
    created->y = player.y + kShoulderHeight;
    created->z = player.z;
    static_cast<void>(world.state().spawnEntity(std::move(created)));
}

void MobActionSystem::tickShoulders(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers) {
    for (PlayerSession* player : watchers) {
        if (!player->shoulderLeft.has_value() && !player->shoulderRight.has_value()) {
            player->shoulderDropPending = false;
            continue;
        }
        // parity: `playShoulderEntityAmbientSound`.
        if (!player->dead) {
            for (const std::optional<wire::nbt::NbtCompound>* shoulder :
                 {&player->shoulderLeft, &player->shoulderRight}) {
                if (!shoulder->has_value() || silent(**shoulder)) {
                    continue;
                }
                random::RandomSource random = freshRandom();
                if (random.nextInt(200) != 0 || (*shoulder)->getString("id") != "minecraft:parrot") {
                    continue;
                }
                if (imitateNearbyMobs(world, player->x, player->y, player->z, kPlayerWidth, kPlayerHeight,
                                      SoundSource::Players, random)) {
                    continue;
                }
                const std::string sound = entity::parrots::ambientSound(world.difficulty() == "peaceful", random);
                soundAt(watchers, sound, SoundSource::Players, player->x, player->y, player->z, 1.0F,
                        entity::parrots::pitch(random), random.nextLong());
            }
        }
        const bool thrown = std::exchange(player->shoulderDropPending, false) || player->fallDistance > kShoulderFall ||
                            feetInWater(world, *player) || player->flying || player->sleepingBed.has_value();
        // parity: `removeEntitiesOnShoulder`, never within a second of sitting down.
        if (!thrown || player->timeEntitySatOnShoulder + kShoulderSettleTicks >= world.gameTime()) {
            continue;
        }
        if (player->shoulderLeft.has_value()) {
            respawnShoulder(world, *player, *std::exchange(player->shoulderLeft, std::nullopt));
        }
        if (player->shoulderRight.has_value()) {
            respawnShoulder(world, *player, *std::exchange(player->shoulderRight, std::nullopt));
        }
    }
}

} // namespace basalt::server::runtime
