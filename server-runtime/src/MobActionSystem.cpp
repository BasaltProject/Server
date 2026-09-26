#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <cmath>
#include <cstdint>
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
constexpr std::int32_t kStrayArrowTicks = 600;
constexpr std::int32_t kBoggedArrowTicks = 100;
constexpr std::string_view kDeathMob = "mob";
constexpr std::string_view kDeathSting = "sting";

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
/// parity: `Cat.IS_LYING`, the one after the variant.
constexpr std::uint8_t kCatLyingIndex = 21;
constexpr std::uint8_t kCatRelaxIndex = 22;
constexpr std::uint8_t kDolphinGotFishIndex = 18;
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
constexpr std::uint8_t kBatFlagsIndex = 16;
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
      options_(std::move(options)),
      villagers_(std::make_unique<VillagerWorldSystem>(
          players, broadcaster, spawning, raids, lootTables, options_.villagerTrades, options_.nextSeed,
          [this](world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                 entity::ServerEntity& mob,
                 const world::ActiveEffect& effect) { static_cast<void>(addMobEffect(world, watchers, mob, effect)); },
          [this](world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y, double z,
                 std::int32_t ownerId) {
              if (hooks_.launchFirework) {
                  hooks_.launchFirework(world, std::move(item), x, y, z, ownerId);
              }
          })) {}

entity::villager::VillagerWorld& MobActionSystem::villagersIn(world::LoadedWorld& world) {
    villagers_->bind(world);
    return *villagers_;
}

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
                                float volume, float pitch, SoundSource source) {
    broadcaster_->sendEffects(
        watchersOf(world), static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
        static_cast<std::int32_t>(std::floor(mob.z)), server::registry::blocks::air().stateId,
        {world::blockEffect::Sound{.name = std::string{sound}, .source = source, .volume = volume, .pitch = pitch}});
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
            wire::protocol::play::SwingAnimationPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::SwingAnimationPacket{.entityId = mob.entityId()}.write(buffer);
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
    if (!wardenSwings(*world, mob)) {
        return;
    }
    if (mob.type().path() == "ravager") {
        ravagerSwings(*world, mob);
    }
    // parity: `Bee.doHurtTarget` stings, with the whole of its attack damage cut to an int.
    const bool bee = entity::bees::isBee(mob.type());
    // parity: `HoglinBase.hurtAndThrowTarget`: half the attack and a random share of the rest.
    const bool tosses = (mob.type().path() == "hoglin" || mob.type().path() == "zoglin") && mob.age >= 0;
    random::RandomSource toss = freshRandom();
    if (tosses && static_cast<std::int32_t>(damage) > 0) {
        damage = (damage / 2.0F) + static_cast<float>(toss.nextInt(static_cast<std::int32_t>(damage)));
    }
    const std::int32_t invulnerableBefore = player->invulnerableTicks;
    const float healthBefore = player->health();
    const float lastHurtBefore = player->lastHurt;
    // parity: `Player.hurtServer`, where the difficulty decides how much of it lands.
    player->connection().hurt(
        player::combat::afterDifficulty(bee ? static_cast<float>(static_cast<std::int32_t>(damage)) : damage,
                                        world->difficulty()),
        bee ? kDeathSting : kDeathMob, bee ? world::damageTypes::kSting : world::damageTypes::kMobAttack,
        entity::DamageSource{
            .entityId = mob.entityId(), .x = mob.x, .z = mob.z, .name = entity::entityCombat::displayName(mob)});
    const bool landed = player->invulnerableTicks != invulnerableBefore || player->health() != healthBefore ||
                        player->lastHurt != lastHurtBefore;
    if (bee && landed) {
        beeStung(*world, mob, player, nullptr);
    }
    if (tosses && landed && player->connection().pushBack) {
        if (const std::optional<entity::shieldBlows::Push> push =
                entity::shieldBlows::hoglinThrow(mob, player->x, player->z, 0.0, toss);
            push.has_value()) {
            player->connection().pushBack(push->x, push->y, push->z, mob.yaw);
        }
    }
    // parity: `OwnerHurtByTargetGoal`: a pet goes for whatever hit the person it follows.
    for (ServerEntity* pet : world->state().entities()) {
        if (pet->owner.has_value() && *pet->owner == player->uuid().toString()) {
            pet->ownerHurtBy = mob.entityId();
        }
    }
}

void MobActionSystem::hurtMob(ServerEntity& mob, ServerEntity& target, float damage) {
    world::LoadedWorld* world = worldOf(target);
    if (world == nullptr || !wardenSwings(*world, mob)) {
        return;
    }
    if (mob.type().path() == "ravager") {
        ravagerSwings(*world, mob);
    }
    entity::entityCombat::blame(target, mob.entityId(), mob.entityId(), mob.type());
    const bool bee = entity::bees::isBee(mob.type());
    const bool tosses = (mob.type().path() == "hoglin" || mob.type().path() == "zoglin") && mob.age >= 0;
    random::RandomSource toss = freshRandom();
    if (tosses && static_cast<std::int32_t>(damage) > 0) {
        damage = (damage / 2.0F) + static_cast<float>(toss.nextInt(static_cast<std::int32_t>(damage)));
    }
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(target, bee ? static_cast<float>(static_cast<std::int32_t>(damage)) : damage, mob.x,
                                   mob.z, hooks_.combat, bee ? "sting" : "mob_attack", mob.type().toString());
    if (!result.landed) {
        return;
    }
    if (bee) {
        beeStung(*world, mob, nullptr, &target);
    }
    if (tosses) {
        if (const std::optional<entity::shieldBlows::Push> push = entity::shieldBlows::hoglinThrow(
                mob, target.x, target.z, target.attributeWithModifiers("minecraft:knockback_resistance"), toss);
            push.has_value()) {
            target.velocityX += push->x;
            target.velocityY += push->y;
            target.velocityZ += push->z;
        }
    }
    target.hurtBy = mob.entityId();
    // parity: a tame wolf's bite counts as its owner's.
    if (mob.type().path() == "wolf" && mob.owner.has_value()) {
        if (const std::optional<wire::common::Uuid> owner = wire::common::Uuid::parse(*mob.owner); owner.has_value()) {
            entity::entityCombat::rememberPlayer(target, *owner);
        }
    }
    target.panicTicks = kPanicTicks;
    const bool infected = result.died && infectVillager(*world, mob, target);
    combat_->announceEntityHit(watchersOf(*world), target,
                               bee ? world::damageTypes::kSting : world::damageTypes::kMobAttack, mob.entityId(),
                               result.died && !infected);
    // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit.
    world::vibrations::emit(world->state(), "entity_damage", target.x, target.y, target.z, world->gameTime(),
                            {.entity = target.uuid()});
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
    // parity: the blaze's and the vex's flag byte both carry it in their lowest bit.
    const bool blaze = mob.type().path() == "blaze" || mob.type().path() == "vex";
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

void MobActionSystem::poseChanged(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    // parity: `Bat.DATA_ID_FLAGS`, whose lowest bit is resting.
    if (mob.type().path() == "bat") {
        sendEntityData(*world, mob.entityId(),
                       SetEntityDataPacket::Entry{.index = kBatFlagsIndex,
                                                  .value = SetEntityDataPacket::ByteValue{
                                                      .value = static_cast<std::int8_t>(mob.flight.resting ? 1 : 0)}});
        return;
    }
    sendEntityData(*world, mob.entityId(),
                   SetEntityDataPacket::Entry{.index = static_cast<std::uint8_t>(entity::taming::kFlagsIndex),
                                              .value = SetEntityDataPacket::ByteValue{
                                                  .value = static_cast<std::int8_t>(entity::taming::flagsOf(mob))}});
    // parity: `Dolphin.GOT_FISH`, which is all a dolphin's pose news is.
    if (mob.type().path() == "dolphin") {
        sendEntityData(*world, mob.entityId(),
                       SetEntityDataPacket::Entry{.index = kDolphinGotFishIndex,
                                                  .value = SetEntityDataPacket::BoolValue{.value = mob.gotFish}});
        return;
    }
    if (mob.type().path() == "cat") {
        sendEntityData(*world, mob.entityId(),
                       SetEntityDataPacket::Entry{.index = kCatLyingIndex,
                                                  .value = SetEntityDataPacket::BoolValue{.value = mob.lying}});
        sendEntityData(*world, mob.entityId(),
                       SetEntityDataPacket::Entry{.index = kCatRelaxIndex,
                                                  .value = SetEntityDataPacket::BoolValue{.value = mob.relaxOne}});
    }
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
        const wire::protocol::play::LevelParticlesPacket crit{.particleTypeId = ExplosionSystem::particleId("crit"),
                                                              .x = x,
                                                              .y = targetY + kFangParticleLift,
                                                              .z = z,
                                                              .count = kFangParticles};
        for (const PlayerSession* watcher : watchers) {
            player::showParticleFor(*watcher, "minecraft:crit", crit);
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
            // parity: `EvokerSummonSpellGoal`: bound where it rose, and twenty to a hundred and twenty
            // seconds to live.
            vex->ownerId = mob.entityId();
            vex->flight.boundOrigin = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(vex->x)),
                                                                  static_cast<std::int32_t>(std::floor(vex->y)),
                                                                  static_cast<std::int32_t>(std::floor(vex->z))};
            const auto roll = options_.nextDouble ? static_cast<std::int32_t>(options_.nextDouble() * 90.0) : 0;
            vex->flight.limitedLifeTicks = 20 * (30 + roll);
        }
    }
}

void MobActionSystem::applyEffect(std::int32_t playerId, std::string_view effect, std::int32_t durationTicks,
                                  std::int32_t amplifier, std::optional<std::int32_t> sourceId) {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->entityId() == playerId && player->connection().addEffect) {
            player->noteEffectSource(sourceId);
            player->connection().addEffect(Identifier::vanilla(std::string{effect}), durationTicks, amplifier);
            return;
        }
    }
}

std::optional<bool> MobActionSystem::blockTagged(std::string_view tag, const wire::common::Identifier& block) const {
    if (options_.blockTags == nullptr) {
        return std::nullopt;
    }
    return world::realBlockTags::contains(*options_.blockTags, tag, block);
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
    // parity: `Creeper.explodeCreeper`, twice as hard when it is charged.
    explosions_->blowUp(*world, watchers, mob.x, mob.y, mob.z, kCreeperExplosionPower * (mob.powered ? 2.0F : 1.0F),
                        "creeper", mob.entityId());
}

void MobActionSystem::shootArrow(ServerEntity& mob, double targetX, double targetY, double targetZ, float power) {
    static_cast<void>(power);
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    // parity: `WitherBoss.performRangedAttack`, skulls rather than arrows.
    if (mob.wither != nullptr || mob.type().path() == "wither") {
        if (hooks_.witherShoots) {
            hooks_.witherShoots(*world, mob);
        }
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
    // parity: `Stray.getArrow` and `Bogged.getArrow`, thirty seconds of slowness or five of poison.
    if (mob.type().path() == "stray" || mob.type().path() == "bogged") {
        const bool stray = mob.type().path() == "stray";
        wire::nbt::NbtCompound effect;
        effect.putString("id", stray ? "minecraft:slowness" : "minecraft:poison");
        effect.putInt("duration", stray ? kStrayArrowTicks : kBoggedArrowTicks);
        wire::nbt::NbtCompound contents;
        contents.set("custom_effects", wire::nbt::NbtTag::ofList(wire::nbt::NbtList{
                                           wire::nbt::NbtType::Compound, {wire::nbt::NbtTag::ofCompound(effect)}}));
        arrow.heldItem = "arrow";
        arrow.heldItemComponents.set("minecraft:potion_contents", wire::nbt::NbtTag::ofCompound(std::move(contents)));
    }
    // parity: `EnchantmentHelper.getPiercingCount`, off the crossbow in its hand.
    if (mob.sourceNbt.has_value()) {
        const wire::nbt::NbtCompound* equipment = mob.sourceNbt->getCompound("equipment");
        const wire::nbt::NbtCompound* hand = equipment == nullptr ? nullptr : equipment->getCompound("mainhand");
        const wire::nbt::NbtCompound* components = hand == nullptr ? nullptr : hand->getCompound("components");
        const wire::nbt::NbtCompound* levels =
            components == nullptr ? nullptr : components->getCompound("minecraft:enchantments");
        if (levels != nullptr) {
            arrow.pierceLevel = static_cast<std::int8_t>(std::clamp(levels->getInt("minecraft:piercing"), 0, 127));
        }
    }
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
