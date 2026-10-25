#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/AreaEffectCloudSystem.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace basalt::server::runtime {
namespace {

namespace dragons = entity::enderDragon;
namespace play = wire::protocol::play;
using entity::ServerEntity;
using player::PlayerSession;

/// parity: the dragon's eye, at 0.85 of its eight blocks, and a player's.
constexpr double kDragonEyeHeight = 6.8;
constexpr double kPlayerEyeHeight = 1.62;
constexpr double kPlayerHeight = 1.8;
/// parity: `LivingEntity.hasLineOfSight`, which gives up past this.
constexpr double kSightRange = 128.0;
/// parity: `EnderDragon.checkCrystals`.
constexpr double kCrystalReach = 32.0;
constexpr float kWingDamage = 5.0F;
constexpr float kHeadDamage = 10.0F;
constexpr std::string_view kDeathMob = "mob";
/// parity: `LevelEvent.SOUND_DRAGON_DEATH` and `PARTICLES_DESTROY_BLOCK`'s explosion counterpart.
constexpr std::int32_t kDragonDeathEvent = 1028;
constexpr std::int32_t kDragonBreaksBlocksEvent = 2008;
constexpr std::int32_t kDeathTicks = 200;
constexpr std::int32_t kFirstDragonExperience = 12000;
constexpr std::int32_t kDragonExperience = 500;
constexpr std::int32_t kExperienceFromTick = 150;
constexpr std::int32_t kDragonShootsEvent = 1017;
constexpr float kBreathRadius = 5.0F;
constexpr std::int32_t kBreathDuration = 200;
constexpr float kFireballSpeed = 0.1F;

using Box = std::array<double, 6>;

[[nodiscard]] Box inflate(const Box& box, double x, double y, double z) {
    return {box[0] - x, box[1] - y, box[2] - z, box[3] + x, box[4] + y, box[5] + z};
}

[[nodiscard]] bool overlaps(const Box& box, double x, double y, double z, double width, double height) {
    const double half = width / 2.0;
    return x + half > box[0] && x - half < box[3] && y + height > box[1] && y < box[4] && z + half > box[2] &&
           z - half < box[5];
}

[[nodiscard]] bool targetable(const PlayerSession& player) {
    return !player.dead && player.gamemode != play::Gamemode::Creative && player.gamemode != play::Gamemode::Spectator;
}

[[nodiscard]] dragons::Target targetOf(const PlayerSession& player) {
    return dragons::Target{
        .entityId = player.entityId(), .x = player.x, .y = player.y, .z = player.z, .height = kPlayerHeight};
}

void sendMotion(PlayerSession& player, double x, double y, double z) {
    if (player.connection().pushBack) {
        player.connection().pushBack(x, y, z, 0.0F);
        return;
    }
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::SetEntityMotionPacket{.entityId = player.entityId(), .velocityX = x, .velocityY = y, .velocityZ = z}
                .write(buffer);
        });
    }
}

} // namespace

EndFightSystem::EndFightSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                               CombatSystem& combat, ExplosionSystem& explosions, AreaEffectCloudSystem& clouds,
                               Options options)
    : players_(&players), broadcaster_(&broadcaster), combat_(&combat), explosions_(&explosions), clouds_(&clouds),
      options_(std::move(options)) {}

dragons::DragonState& EndFightSystem::stateOf(ServerEntity& dragon) {
    if (dragon.dragon != nullptr) {
        return *dragon.dragon;
    }
    dragon.dragon = std::make_shared<dragons::DragonState>(dragons::freshState(dragon));
    dragons::DragonState& state = *dragon.dragon;
    state.random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    // parity: `EnderDragon.readAdditionalSaveData`.
    if (dragon.sourceNbt.has_value()) {
        const wire::nbt::NbtCompound& saved = *dragon.sourceNbt;
        if (saved.contains(dragons::kPhaseKey)) {
            dragons::setPhase(state, dragons::phaseById(saved.getInt(dragons::kPhaseKey)));
        }
        state.deathTime = saved.getInt(dragons::kDeathTimeKey);
        state.sittingDamageReceived = saved.getFloat(dragons::kSittingDamageKey);
    }
    return state;
}

dragons::DragonWorld EndFightSystem::worldFor(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                              const ServerEntity& dragon, dragons::DragonState& state) {
    dragons::DragonWorld view;
    view.random = &state.random;
    // parity: a dragon only knows the crystal count of the fight it belongs to.
    if (const world::DragonFight* fight = fightOf(world, dragon); fight != nullptr) {
        view.aliveCrystals = fight->aliveCrystals;
    }
    const auto height = [&world](std::int32_t x, std::int32_t z, server::world::HeightmapType type) {
        server::world::Chunk& chunk = world.state().getOrGenerateChunk(x >> 4, z >> 4);
        return chunk.heightAt(x & 15, z & 15, type);
    };
    view.heightNoLeaves = [height](std::int32_t x, std::int32_t z) {
        return height(x, z, server::world::HeightmapType::MotionBlockingNoLeaves);
    };
    view.heightMotionBlocking = [height](std::int32_t x, std::int32_t z) {
        return height(x, z, server::world::HeightmapType::MotionBlocking);
    };
    view.canSee = [&world, &dragon](const dragons::Target& target) {
        const double fromY = dragon.y + kDragonEyeHeight;
        const double toY = target.y + kPlayerEyeHeight;
        const double dx = target.x - dragon.x;
        const double dy = toY - fromY;
        const double dz = target.z - dragon.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) > kSightRange * kSightRange) {
            return false;
        }
        return !entity::blockRay::cast(world.state(), dragon.x, fromY, dragon.z, target.x, toY, target.z).has_value();
    };
    view.nearestPlayer = [&watchers, &dragon,
                          canSee = view.canSee](double x, double y, double z,
                                                const dragons::TargetQuery& query) -> std::optional<dragons::Target> {
        std::optional<dragons::Target> best;
        double bestDistance = std::numeric_limits<double>::max();
        for (const PlayerSession* player : watchers) {
            if (!targetable(*player)) {
                continue;
            }
            const dragons::Target target = targetOf(*player);
            if (query.range > 0.0) {
                // parity: `TargetingConditions.test`.
                const double reach = std::max(query.range * (player->sneaking ? 0.8 : 1.0), 2.0);
                const double apart = ((dragon.x - player->x) * (dragon.x - player->x)) +
                                     ((dragon.y - player->y) * (dragon.y - player->y)) +
                                     ((dragon.z - player->z) * (dragon.z - player->z));
                if (apart > reach * reach) {
                    continue;
                }
            }
            if (query.maxHeightGap.has_value() && std::abs(player->y - dragon.y) > *query.maxHeightGap) {
                continue;
            }
            if (query.needsSight && !canSee(target)) {
                continue;
            }
            const double distance = ((player->x - x) * (player->x - x)) + ((player->y - y) * (player->y - y)) +
                                    ((player->z - z) * (player->z - z));
            if (distance < bestDistance) {
                bestDistance = distance;
                best = target;
            }
        }
        return best;
    };
    view.playerById = [&watchers](std::int32_t entityId) -> std::optional<dragons::Target> {
        for (const PlayerSession* player : watchers) {
            if (player->entityId() == entityId && !player->dead) {
                return targetOf(*player);
            }
        }
        return std::nullopt;
    };
    view.breathe = [this, &world, id = dragon.entityId()](const dragons::Vec3& at) {
        // parity: `DragonSittingFlamingPhase`.
        return clouds_->spawn(world, entity::areaEffectClouds::dragonBreath(kBreathRadius, kBreathDuration, 0.0F, 0),
                              at.x, at.y, at.z, id);
    };
    view.stopBreathing = [&world](std::int32_t cloudId) {
        if (ServerEntity* cloud = world.state().entity(cloudId); cloud != nullptr) {
            cloud->removed = true;
        }
    };
    view.shootFireball = [this, &world, &watchers, &dragon](const dragons::Vec3& from, const dragons::Vec3& towards) {
        const entity::projectiles::Kind* kind =
            entity::projectiles::kindOf(wire::common::Identifier::vanilla("dragon_fireball"));
        const server::registry::EntityTypeEntry* entry =
            kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
        if (entry == nullptr) {
            return;
        }
        const bool silent = dragon.sourceNbt.has_value() && dragon.sourceNbt->getBoolean("Silent");
        if (!silent) {
            // parity: `LevelEvent.SOUND_DRAGON_FIREBALL`, from the dragon's own block.
            for (PlayerSession* player : watchers) {
                if (player->connection().sendLevelEvent) {
                    player->connection().sendLevelEvent(kDragonShootsEvent, wire::common::mth::floor(dragon.x),
                                                        wire::common::mth::floor(dragon.y),
                                                        wire::common::mth::floor(dragon.z), 0);
                }
            }
        }
        random::RandomSource spread = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
        // parity: `assignDirectionalMovement`, the direction at the tenth of a block a tick every.
        // hurting projectile starts with.
        ServerEntity& ball =
            entity::projectiles::spawn(world.state(), *kind, players_->allocateEntityId(), entry->protocolId, from.x,
                                       from.y, from.z, towards.x, towards.y, towards.z, kFireballSpeed, 0.0F, spread);
        ball.ownerId = dragon.entityId();
    };
    view.isEmptyBlock = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
        return !state.has_value() || server::registry::blocks::isAir(*state);
    };
    return view;
}

void EndFightSystem::tickDragon(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                const std::vector<ServerEntity*>& living, ServerEntity& dragon) {
    dragons::DragonState& state = stateOf(dragon);
    dragon.tickCount++;
    entity::entityCombat::tick(dragon);
    if (dragon.health <= 0.0) {
        tickDeath(world, dragon, state);
        return;
    }
    for (const std::int32_t stale : state.staleClouds) {
        if (ServerEntity* cloud = world.state().entity(stale); cloud != nullptr) {
            cloud->removed = true;
        }
    }
    state.staleClouds.clear();
    dragons::DragonWorld view = worldFor(world, watchers, dragon, state);
    dragons::buildNodes(state, view.heightNoLeaves);
    checkCrystals(living, dragon, state);
    dragons::steer(dragon, state, view);
    dragons::placeBodyAndWings(dragon, state);
    // parity: `LivingEntity.hurtTime`.
    if (dragon.hurtCooldown <= entity::entityCombat::kHurtCooldownTicks / 2) {
        strike(world, watchers, living, dragon, state);
    }
    dragons::placeRest(dragon, state);
    const bool head = checkWalls(world, watchers, state, dragons::partBox(state, dragons::Part::Head));
    const bool neck = checkWalls(world, watchers, state, dragons::partBox(state, dragons::Part::Neck));
    const bool body = checkWalls(world, watchers, state, dragons::partBox(state, dragons::Part::Body));
    state.inWall = head || neck || body;
    dragon.headYaw = dragon.yaw;
    updateDragon(world, dragon);
    if (!dragon.sourceNbt.has_value()) {
        dragon.sourceNbt.emplace();
    }
    dragon.sourceNbt->putInt(dragons::kPhaseKey, static_cast<std::int32_t>(state.phase));
    dragon.sourceNbt->putInt(dragons::kDeathTimeKey, state.deathTime);
    dragon.sourceNbt->putFloat(dragons::kSittingDamageKey, state.sittingDamageReceived);
    // parity: `Mob.checkDespawn` is empty for a dragon.
    dragon.persistent = true;
}

void EndFightSystem::checkCrystals(const std::vector<ServerEntity*>& living, ServerEntity& dragon,
                                   dragons::DragonState& state) {
    if (state.nearestCrystal.has_value()) {
        const auto found = std::ranges::find_if(
            living, [&state](const ServerEntity* candidate) { return candidate->entityId() == *state.nearestCrystal; });
        if (found == living.end() || (*found)->removed) {
            state.nearestCrystal.reset();
        } else if (dragon.tickCount % 10 == 0) {
            const double max = dragon.attributeWithModifiers(entity::attribute::kMaxHealth);
            if (dragon.health < max) {
                dragon.health = std::min(max, dragon.health + 1.0);
            }
        }
    }
    if (state.random.nextInt(10) != 0) {
        return;
    }
    // parity: the dragon's own sixteen by eight box, grown by 32 on every side.
    const Box reach =
        inflate({dragon.x - 8.0, dragon.y, dragon.z - 8.0, dragon.x + 8.0, dragon.y + 8.0, dragon.z + 8.0},
                kCrystalReach, kCrystalReach, kCrystalReach);
    std::optional<std::int32_t> nearest;
    double nearestDistance = std::numeric_limits<double>::max();
    for (const ServerEntity* candidate : living) {
        if (candidate->removed || !world::endCrystals::isCrystal(candidate->type()) ||
            !overlaps(reach, candidate->x, candidate->y, candidate->z, 2.0, 2.0)) {
            continue;
        }
        const double distance = ((candidate->x - dragon.x) * (candidate->x - dragon.x)) +
                                ((candidate->y - dragon.y) * (candidate->y - dragon.y)) +
                                ((candidate->z - dragon.z) * (candidate->z - dragon.z));
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = candidate->entityId();
        }
    }
    state.nearestCrystal = nearest;
}

void EndFightSystem::strike(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                            const std::vector<ServerEntity*>& living, ServerEntity& dragon,
                            dragons::DragonState& state) {
    const Box bodyBox = dragons::partBox(state, dragons::Part::Body);
    const double centreX = (bodyBox[0] + bodyBox[3]) / 2.0;
    const double centreZ = (bodyBox[2] + bodyBox[5]) / 2.0;
    const bool sitting = dragons::isSitting(state.phase);
    const auto hurtPlayer = [&world, &dragon](PlayerSession& player, float damage) {
        if (!player.connection().hurt) {
            return;
        }
        player.connection().hurt(player::combat::afterDifficulty(damage, world.difficulty()), kDeathMob,
                                 world::damageTypes::kMobAttack,
                                 entity::DamageSource{.entityId = dragon.entityId(),
                                                      .x = dragon.x,
                                                      .z = dragon.z,
                                                      .name = entity::entityCombat::displayName(dragon)});
    };
    const auto hurtMob = [this, &watchers, &dragon](ServerEntity& mob, float damage) {
        entity::entityCombat::blame(mob, dragon.entityId(), dragon.entityId(), dragon.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, damage, dragon.x, dragon.z, {}, "mob_attack", dragon.type().toString());
        if (result.landed) {
            combat_->announceEntityHit(watchers, mob, world::damageTypes::kMobAttack, dragon.entityId(), result.died);
        }
    };
    for (const dragons::Part wing : {dragons::Part::Wing1, dragons::Part::Wing2}) {
        Box area = inflate(dragons::partBox(state, wing), 4.0, 2.0, 4.0);
        area[1] -= 2.0;
        area[4] -= 2.0;
        const auto shove = [&](double x, double z) {
            const double xd = x - centreX;
            const double zd = z - centreZ;
            const double spread = std::max((xd * xd) + (zd * zd), 0.1);
            return std::array<double, 3>{xd / spread * 4.0, static_cast<double>(0.2F), zd / spread * 4.0};
        };
        for (PlayerSession* player : watchers) {
            if (!targetable(*player) || !overlaps(area, player->x, player->y, player->z, 0.6, kPlayerHeight)) {
                continue;
            }
            const std::array<double, 3> push = shove(player->x, player->z);
            sendMotion(*player, push[0], push[1], push[2]);
            if (!sitting) {
                hurtPlayer(*player, kWingDamage);
            }
        }
        for (ServerEntity* mob : living) {
            if (mob == &dragon || mob->removed || mob->dying ||
                !server::registry::entityAttributes::isLiving(mob->type()) ||
                !overlaps(area, mob->x, mob->y, mob->z, mob->width(), mob->height())) {
                continue;
            }
            const std::array<double, 3> push = shove(mob->x, mob->z);
            mob->velocityX += push[0];
            mob->velocityY += push[1];
            mob->velocityZ += push[2];
            if (!sitting) {
                hurtMob(*mob, kWingDamage);
            }
        }
    }
    for (const dragons::Part biting : {dragons::Part::Head, dragons::Part::Neck}) {
        const Box area = inflate(dragons::partBox(state, biting), 1.0, 1.0, 1.0);
        for (PlayerSession* player : watchers) {
            if (targetable(*player) && overlaps(area, player->x, player->y, player->z, 0.6, kPlayerHeight)) {
                hurtPlayer(*player, kHeadDamage);
            }
        }
        for (ServerEntity* mob : living) {
            if (mob != &dragon && !mob->removed && !mob->dying &&
                server::registry::entityAttributes::isLiving(mob->type()) &&
                overlaps(area, mob->x, mob->y, mob->z, mob->width(), mob->height())) {
                hurtMob(*mob, kHeadDamage);
            }
        }
    }
}

bool EndFightSystem::checkWalls(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                dragons::DragonState& state, const Box& box) {
    namespace mth = wire::common::mth;
    const std::int32_t x0 = mth::floor(box[0]);
    const std::int32_t y0 = mth::floor(box[1]);
    const std::int32_t z0 = mth::floor(box[2]);
    const std::int32_t x1 = mth::floor(box[3]);
    const std::int32_t y1 = mth::floor(box[4]);
    const std::int32_t z1 = mth::floor(box[5]);
    const bool griefing = world.gameRules().get(world::gameRules::kMobGriefing);
    const auto tagged = [this](std::int32_t stateId, std::string_view tag) {
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return options_.blockTags != nullptr && block != nullptr &&
               world::realBlockTags::contains(*options_.blockTags, tag, *block);
    };
    bool hitWall = false;
    bool destroyed = false;
    for (std::int32_t x = x0; x <= x1; ++x) {
        for (std::int32_t y = y0; y <= y1; ++y) {
            for (std::int32_t z = z0; z <= z1; ++z) {
                const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                if (!stateId.has_value() || server::registry::blocks::isAir(*stateId) ||
                    tagged(*stateId, "minecraft:dragon_transparent")) {
                    continue;
                }
                if (!griefing || tagged(*stateId, "minecraft:dragon_immune")) {
                    hitWall = true;
                    continue;
                }
                const std::int32_t air = server::registry::blocks::air().stateId;
                if (world.state().setBlockState(x, y, z, air)) {
                    destroyed = true;
                    broadcaster_->announceRedstone(world, watchers, world::BlockChange{x, y, z, air, {}});
                }
            }
        }
    }
    if (destroyed) {
        const std::int32_t x = x0 + state.random.nextInt(x1 - x0 + 1);
        const std::int32_t y = y0 + state.random.nextInt(y1 - y0 + 1);
        const std::int32_t z = z0 + state.random.nextInt(z1 - z0 + 1);
        for (PlayerSession* player : watchers) {
            if (player->connection().sendLevelEvent) {
                player->connection().sendLevelEvent(kDragonBreaksBlocksEvent, x, y, z, 0);
            }
        }
    }
    return hitWall;
}

void EndFightSystem::tickDeath(world::LoadedWorld& world, ServerEntity& dragon, dragons::DragonState& state) {
    updateDragon(world, dragon);
    state.deathTime++;
    const bool silent = dragon.sourceNbt.has_value() && dragon.sourceNbt->getBoolean("Silent");
    if (state.deathTime == 1 && !silent) {
        // parity: `globalLevelEvent`, heard by every player on the server wherever they are.
        const play::LevelEventPacket event{.event = kDragonDeathEvent,
                                           .pos = play::BlockPos{wire::common::mth::floor(dragon.x),
                                                                 wire::common::mth::floor(dragon.y),
                                                                 wire::common::mth::floor(dragon.z)},
                                           .data = 0,
                                           .global = true};
        // Not vanilla when a radius is set: the sound reaches only players that close.
        const double radius = world.settings().rules.gameplay.dragonDeathSoundRadius;
        for (PlayerSession* player : players_->snapshot()) {
            if (radius > 0.0 && (player->worldName != world.name() || std::abs(player->x - dragon.x) > radius ||
                                 std::abs(player->y - dragon.y) > radius || std::abs(player->z - dragon.z) > radius)) {
                continue;
            }
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(play::LevelEventPacket::kId,
                                                    [&event](wire::io::ByteBuffer& buffer) { event.write(buffer); });
            }
        }
    }
    // parity: `tickDeath`'s experience.
    // eight percent every fifth tick from 150 and a fifth at the end.
    const world::DragonFight* fight = fightOf(world, dragon);
    const std::int32_t worth =
        fight != nullptr && !fight->previouslyKilled ? kFirstDragonExperience : kDragonExperience;
    const bool drops = world.gameRules().get(world::gameRules::kMobDrops) && options_.awardExperience;
    if (drops && state.deathTime > kExperienceFromTick && state.deathTime % 5 == 0) {
        options_.awardExperience(world, dragon.x, dragon.y, dragon.z,
                                 wire::common::mth::floor(static_cast<double>(static_cast<float>(worth) * 0.08F)));
    }
    // parity: the corpse rises a tenth of a block a tick, parts and all.
    dragon.y += 0.1;
    for (dragons::Vec3& part : state.parts) {
        part.y += 0.1;
    }
    if (dragon.sourceNbt.has_value()) {
        dragon.sourceNbt->putInt(dragons::kDeathTimeKey, state.deathTime);
    }
    if (state.deathTime >= kDeathTicks) {
        if (drops) {
            options_.awardExperience(world, dragon.x, dragon.y, dragon.z,
                                     wire::common::mth::floor(static_cast<double>(static_cast<float>(worth) * 0.2F)));
        }
        dragonKilled(world, dragon);
        dragon.removed = true;
        dragon.killed = true;
    }
}

} // namespace basalt::server::runtime
