#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <cmath>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::SetEntityDataPacket;
using wire::protocol::play::SoundSource;

/// parity: `EntityType.PLAYER`, whose box is what a mob's reach is measured against.
const Identifier& playerType() {
    static const Identifier kType = Identifier::vanilla("player");
    return kType;
}

/// parity: `death.attack.mob`, the message a mob with no name kills you with.
constexpr std::string_view kDeathMob = "mob";

/// parity: `PanicGoal`, which runs for as long as the last hit is remembered.
constexpr std::int32_t kPanicTicks = 40;

/// parity: `Creeper.explosionRadius`, and the sounds a fuse, a bow and a teleport make.
constexpr float kCreeperExplosionPower = 3.0F;
constexpr std::string_view kSoundCreeperPrimed = "entity.creeper.primed";
constexpr std::string_view kSoundEndermanTeleport = "entity.enderman.teleport";
constexpr std::string_view kSoundFireballShoot = "entity.ghast.shoot";
constexpr std::string_view kSoundWitchThrow = "entity.witch.throw";
constexpr std::string_view kSoundFangs = "entity.evoker.prepare_attack";
constexpr std::string_view kSoundSkeletonShoot = "entity.skeleton.shoot";

/// parity: `SpellcasterIllager`'s own spell byte, and the pose it puts the model in.
constexpr std::uint8_t kSpellIndex = 17;
constexpr std::int8_t kSpellCasting = 1;
constexpr std::int8_t kSpellNone = 0;

/// parity: `Witch.performRangedAttack`, thrown at 0.75 with eight degrees of spread.
constexpr float kPotionPower = 0.75F;
constexpr float kPotionSpread = 8.0F;
constexpr double kPotionArc = 0.2;
constexpr double kPotionThrowShare = 0.5;

/// parity: `EvokerAttackSpellGoal`, sixteen fangs walking out at six damage each.
constexpr std::int32_t kFangSteps = 16;
constexpr double kFangStart = 1.5;
constexpr double kFangSpacing = 1.0;
constexpr double kFangHitRange = 1.0;
constexpr float kFangDamage = 6.0F;
constexpr std::int32_t kFangParticles = 4;
constexpr double kFangParticleLift = 0.5;

/// parity: `Ghast.DATA_IS_CHARGING`, `Blaze`'s own flags byte and `Guardian`'s beam target.
constexpr std::uint8_t kGhastChargingIndex = 16;
constexpr std::uint8_t kBlazeChargedIndex = 16;
constexpr std::int8_t kBlazeChargedFlag = 1;
constexpr std::uint8_t kGuardianBeamIndex = 17;

/// parity: `Ghast.getExplosionPower`, and the speed a fireball is thrown at.
constexpr float kGhastExplosionPower = 1.0F;
constexpr float kFireballPower = 0.5F;
constexpr float kSmallFireballSpread = 4.0F;
constexpr double kFireballEyeShare = 0.5;

/// parity: `performRangedAttack`: from the eyes, aimed a third up the target, at 1.6.
constexpr double kArrowEyeShare = 0.85;
constexpr double kArrowAimShare = 1.0 / 3.0;
constexpr double kArrowArc = 0.2;
constexpr float kArrowPower = 1.6F;

/// parity: `14 - difficulty * 4`, the spread a skeleton's shot has.
float arrowSpread(const world::LoadedWorld& world) {
    const std::string& difficulty = world.difficulty();
    if (difficulty == "peaceful") {
        return 14.0F;
    }
    if (difficulty == "easy") {
        return 10.0F;
    }
    if (difficulty == "normal") {
        return 6.0F;
    }
    return 2.0F;
}

} // namespace

MobActionSystem::MobActionSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                                 ExplosionSystem& explosions, NaturalSpawningSystem& spawning, CombatSystem& combat,
                                 RaidSystem& raids, world::RealLootTables& lootTables, Worlds worlds, Hooks hooks,
                                 Options options)
    : players_(&players), broadcaster_(&broadcaster), explosions_(&explosions), spawning_(&spawning), combat_(&combat),
      raids_(&raids), lootTables_(&lootTables), worlds_(std::move(worlds)), hooks_(std::move(hooks)),
      options_(std::move(options)) {}

random::RandomSource MobActionSystem::freshRandom() const {
    return random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
}

world::LoadedWorld* MobActionSystem::worldOf(const ServerEntity& mob) {
    if (!worlds_) {
        return nullptr;
    }
    for (world::LoadedWorld* world : worlds_()) {
        if (world != nullptr && world->state().entity(mob.entityId()) != nullptr) {
            return world;
        }
    }
    return nullptr;
}

std::vector<PlayerSession*> MobActionSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

void MobActionSystem::sendEntityData(const world::LoadedWorld& world, std::int32_t entityId,
                                     const SetEntityDataPacket::Entry& entry) {
    for (PlayerSession* player : watchersOf(world)) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetEntityDataPacket{.entityId = entityId, .entries = {entry}}.write(buffer);
        });
    }
}

void MobActionSystem::playSound(const world::LoadedWorld& world, const ServerEntity& mob, std::string_view sound,
                                float volume, float pitch) {
    broadcaster_->sendEffects(
        watchersOf(world), static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
        static_cast<std::int32_t>(std::floor(mob.z)), server::registry::blocks::air().stateId,
        {world::blockEffect::Sound{
            .name = std::string{sound}, .source = SoundSource::Hostile, .volume = volume, .pitch = pitch}});
}

void MobActionSystem::swing(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    for (PlayerSession* player : watchersOf(*world)) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(
            wire::protocol::play::AnimatePacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::AnimatePacket{.entityId = mob.entityId(),
                                                    .action = wire::protocol::play::AnimatePacket::kSwingMainHand}
                    .write(buffer);
            });
    }
}

void MobActionSystem::hurtPlayer(ServerEntity& mob, std::int32_t playerId, float damage) {
    PlayerSession* player = nullptr;
    for (PlayerSession* candidate : players_->snapshot()) {
        if (candidate->entityId() == playerId) {
            player = candidate;
            break;
        }
    }
    if (player == nullptr) {
        return;
    }
    world::LoadedWorld* world = nullptr;
    for (world::LoadedWorld* candidate : worlds_()) {
        if (candidate != nullptr && candidate->name() == player->worldName) {
            world = candidate;
            break;
        }
    }
    if (world == nullptr || !player->connection().hurt) {
        return;
    }
    // parity: `Player.hurtServer`, where the difficulty decides how much of it lands.
    player->connection().hurt(
        player::combat::afterDifficulty(damage, world->difficulty()), kDeathMob, world::damageTypes::kMobAttack,
        entity::DamageSource{
            .entityId = mob.entityId(), .x = mob.x, .z = mob.z, .name = entity::entityCombat::displayName(mob)});
    // parity: `OwnerHurtByTargetGoal`: a pet goes for whatever hit the person it follows.
    for (ServerEntity* pet : world->state().entities()) {
        if (pet->owner.has_value() && *pet->owner == player->uuid().toString()) {
            pet->ownerHurtBy = mob.entityId();
        }
    }
}

void MobActionSystem::hurtMob(ServerEntity& mob, ServerEntity& target, float damage) {
    world::LoadedWorld* world = worldOf(target);
    if (world == nullptr) {
        return;
    }
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(target, damage, mob.x, mob.z, hooks_.combat, "mob_attack", mob.type().toString());
    if (!result.landed) {
        return;
    }
    target.hurtBy = mob.entityId();
    target.panicTicks = kPanicTicks;
    combat_->announceEntityHit(watchersOf(*world), target, world::damageTypes::kMobAttack, mob.entityId(), result.died);
}

void MobActionSystem::breed(ServerEntity& mob, ServerEntity& partner) {
    world::LoadedWorld* world = worldOf(mob);
    if (world != nullptr) {
        breed(*world, mob, partner);
    }
}

void MobActionSystem::eatGrass(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world != nullptr) {
        eatGrassUnder(*world, mob);
    }
}

void MobActionSystem::setCharging(ServerEntity& mob, bool charging) {
    if (mob.charging == charging) {
        return;
    }
    mob.charging = charging;
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const bool blaze = mob.type().path() == "blaze";
    const SetEntityDataPacket::Entry entry =
        blaze ? SetEntityDataPacket::Entry{.index = kBlazeChargedIndex,
                                           .value = SetEntityDataPacket::ByteValue{.value = charging ? kBlazeChargedFlag
                                                                                                     : std::int8_t{0}}}
              : SetEntityDataPacket::Entry{.index = kGhastChargingIndex,
                                           .value = SetEntityDataPacket::BoolValue{.value = charging}};
    sendEntityData(*world, mob.entityId(), entry);
}

void MobActionSystem::shootFireball(ServerEntity& mob, double targetX, double targetY, double targetZ, bool large) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const entity::projectiles::Kind& kind =
        large ? entity::projectiles::fireball() : *entity::projectiles::kindOf(Identifier::vanilla("small_fireball"));
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(kind.type);
    if (entry == nullptr) {
        return;
    }
    const double fromY = mob.y + (mob.height() * kFireballEyeShare);
    random::RandomSource random = freshRandom();
    ServerEntity& ball = entity::projectiles::spawn(
        world->state(), kind, players_->allocateEntityId(), entry->protocolId, mob.x, fromY, mob.z, targetX - mob.x,
        targetY - fromY, targetZ - mob.z, kFireballPower, large ? 0.0F : kSmallFireballSpread, random);
    ball.ownerId = mob.entityId();
    ball.explosionPower = large ? kGhastExplosionPower : 0.0F;
    playSound(*world, mob, kSoundFireballShoot);
}

void MobActionSystem::setBeamTarget(ServerEntity& mob, std::optional<std::int32_t> targetId) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    sendEntityData(*world, mob.entityId(),
                   SetEntityDataPacket::Entry{.index = kGuardianBeamIndex,
                                              .value = SetEntityDataPacket::IntValue{.value = targetId.value_or(0)}});
}

void MobActionSystem::setSleeping(ServerEntity& mob, bool sleeping) {
    if (mob.sleeping == sleeping) {
        return;
    }
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    mob.sleeping = sleeping;
    if (sleeping && mob.type().path() == "villager") {
        mob.lastSleptGameTime = world->gameTime();
    }
    sendEntityData(*world, mob.entityId(),
                   SetEntityDataPacket::Entry{.index = SetEntityDataPacket::kPoseIndex,
                                              .value = SetEntityDataPacket::PoseValue{
                                                  .pose = sleeping ? wire::protocol::play::Pose::Sleeping
                                                                   : wire::protocol::play::Pose::Standing}});
}

void MobActionSystem::setCasting(ServerEntity& mob, bool casting) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    sendEntityData(*world, mob.entityId(),
                   SetEntityDataPacket::Entry{
                       .index = kSpellIndex,
                       .value = SetEntityDataPacket::ByteValue{.value = casting ? kSpellCasting : kSpellNone}});
}

void MobActionSystem::throwPotion(ServerEntity& mob, double targetX, double targetY, double targetZ,
                                  std::string_view potion) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const entity::projectiles::Kind* kind = entity::projectiles::kindOf(Identifier::vanilla("potion"));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
    if (entry == nullptr) {
        return;
    }
    const double fromY = mob.y + (mob.height() * kPotionThrowShare);
    const double dx = targetX - mob.x;
    const double dz = targetZ - mob.z;
    const double flat = std::sqrt((dx * dx) + (dz * dz));
    random::RandomSource random = freshRandom();
    // parity: `performRangedAttack`'s own arc, a fifth of the distance across.
    ServerEntity& bottle = entity::projectiles::spawn(
        world->state(), *kind, players_->allocateEntityId(), entry->protocolId, mob.x, fromY, mob.z, dx,
        targetY - fromY + (flat * kPotionArc), dz, kPotionPower, kPotionSpread, random);
    bottle.ownerId = mob.entityId();
    bottle.potionEffect = std::string{potion};
    playSound(*world, mob, kSoundWitchThrow);
}

void MobActionSystem::castFangs(ServerEntity& mob, double targetX, double targetY, double targetZ) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(*world);
    // parity: `Evoker.EvokerAttackSpellGoal`, two lines of fangs walking towards you.
    const double angle = std::atan2(targetZ - mob.z, targetX - mob.x);
    for (std::int32_t step = 0; step < kFangSteps; step++) {
        const double reach = kFangStart + (step * kFangSpacing);
        const double x = mob.x + (std::cos(angle) * reach);
        const double z = mob.z + (std::sin(angle) * reach);
        for (PlayerSession* player : watchers) {
            const double dx = player->x - x;
            const double dz = player->z - z;
            if ((dx * dx) + (dz * dz) > kFangHitRange || !player->connection().hurt) {
                continue;
            }
            player->connection().hurt(kFangDamage, kDeathMob, world::damageTypes::kMobAttack,
                                      entity::DamageSource{.entityId = mob.entityId(),
                                                           .x = mob.x,
                                                           .z = mob.z,
                                                           .name = entity::entityCombat::displayName(mob)});
        }
        for (PlayerSession* watcher : watchers) {
            if (!watcher->connection().sendPlayPacket) {
                continue;
            }
            watcher->connection().sendPlayPacket(
                wire::protocol::play::LevelParticlesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::LevelParticlesPacket{.particleTypeId = ExplosionSystem::particleId("crit"),
                                                               .x = x,
                                                               .y = targetY + kFangParticleLift,
                                                               .z = z,
                                                               .count = kFangParticles}
                        .write(buffer);
                });
        }
    }
    playSound(*world, mob, kSoundFangs);
}

void MobActionSystem::summonVexes(ServerEntity& mob, std::int32_t count) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    static const Identifier kVexType = Identifier::vanilla("vex");
    for (std::int32_t index = 0; index < count; index++) {
        const double offsetX = options_.nextDouble ? (options_.nextDouble() * 2.0) - 1.0 : 0.0;
        const double offsetZ = options_.nextDouble ? (options_.nextDouble() * 2.0) - 1.0 : 0.0;
        ServerEntity* vex = spawning_->spawnMob(*world, kVexType, mob.x + offsetX, mob.y + 1.0, mob.z + offsetZ);
        if (vex != nullptr) {
            vex->targetId = mob.targetId;
        }
    }
}

void MobActionSystem::applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks,
                                  std::int32_t amplifier) {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->entityId() == playerId && player->connection().addEffect) {
            player->connection().addEffect(Identifier::vanilla(std::string{effect}), durationTicks, amplifier);
            return;
        }
    }
}

void MobActionSystem::teleported(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    playSound(*world, mob, kSoundEndermanTeleport);
    // The client has to be told where it went: it is a jump, not a walk.
    for (PlayerSession* player : watchersOf(*world)) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(wire::protocol::play::EntityTeleportPacket::kId,
                                            [&](wire::io::ByteBuffer& buffer) {
                                                wire::protocol::play::EntityTeleportPacket{.entityId = mob.entityId(),
                                                                                           .x = mob.x,
                                                                                           .y = mob.y,
                                                                                           .z = mob.z,
                                                                                           .yaw = mob.yaw,
                                                                                           .pitch = mob.pitch,
                                                                                           .onGround = true}
                                                    .write(buffer);
                                            });
    }
}

void MobActionSystem::primeCreeper(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world != nullptr) {
        playSound(*world, mob, kSoundCreeperPrimed, 1.0F, 0.5F);
    }
}

void MobActionSystem::explodeCreeper(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(*world);
    mob.removed = true;
    explosions_->blowUp(*world, watchers, mob.x, mob.y, mob.z, kCreeperExplosionPower, "creeper", mob.entityId());
}

void MobActionSystem::shootArrow(ServerEntity& mob, double targetX, double targetY, double targetZ, float power) {
    static_cast<void>(power);
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    const entity::projectiles::Kind& kind = entity::projectiles::arrow();
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(kind.type);
    if (entry == nullptr) {
        return;
    }
    const double fromY = mob.y + (mob.height() * kArrowEyeShare);
    const double dx = targetX - mob.x;
    const double dy = targetY + kArrowAimShare - fromY;
    const double dz = targetZ - mob.z;
    const double flat = std::sqrt((dx * dx) + (dz * dz));
    random::RandomSource random = freshRandom();
    // parity: the arc a skeleton adds, a fifth of the distance to the target.
    ServerEntity& arrow =
        entity::projectiles::spawn(world->state(), kind, players_->allocateEntityId(), entry->protocolId, mob.x, fromY,
                                   mob.z, dx, dy + (flat * kArrowArc), dz, kArrowPower, arrowSpread(*world), random);
    arrow.ownerId = mob.entityId();
    playSound(*world, mob, kSoundSkeletonShoot);
}

bool MobActionSystem::touchesCart(const ServerEntity& cart, const PlayerSession& player) {
    const double reach = (cart.width() + server::registry::entityTypeShapes::widthOf(playerType())) / 2.0;
    if (std::abs(player.x - cart.x) > reach || std::abs(player.z - cart.z) > reach) {
        return false;
    }
    return player.y <= cart.y + cart.height() &&
           player.y + server::registry::entityTypeShapes::heightOf(playerType()) >= cart.y;
}

} // namespace basalt::server::runtime
