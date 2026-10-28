#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace neutral = entity::neutralAnger;

/// parity: `ZombifiedPiglin`'s constants.
constexpr std::int32_t kAngerMin = 20 * 20;
constexpr std::int32_t kAngerMax = 39 * 20;
constexpr std::int32_t kFirstSoundMax = 20;
constexpr std::int32_t kAlertMin = 4 * 20;
constexpr std::int32_t kAlertMax = 6 * 20;
constexpr double kAlertRangeY = 10.0;
/// parity: `SPEED_MODIFIER_ATTACKING`, a flat 0.05 while angry.
constexpr std::string_view kAttackingModifier = "minecraft:attacking";
/// parity: `Enderman.SPEED_MODIFIER_ATTACKING`, 0.15 while it has a target.
constexpr double kEndermanAttackingSpeed = static_cast<double>(0.15F);
/// parity: `tellNeutralMobsThatIDied`, 32 across and 10 up or down of where they died.
constexpr double kForgiveRange = 32.0;
constexpr double kForgiveRangeY = 10.0;

} // namespace

void MobActionSystem::updateNeutralAnger(world::LoadedWorld& world, ServerEntity& mob, neutral::Anger& anger,
                                         random::RandomSource& random, bool stayAngry, std::int32_t minTicks,
                                         std::int32_t maxTicks) {
    struct Living {
        wire::common::Uuid uuid;
        bool alive = false;
        bool player = false;
        bool validPlayer = false;
    };

    const bool peaceful = world.difficulty() == "peaceful";
    const auto resolve = [&](std::int32_t id) -> std::optional<Living> {
        for (PlayerSession* player : players_->snapshot()) {
            if (player->entityId() == id && player->worldName == world.name()) {
                const bool exempt = player->gamemode == Gamemode::Creative || player->gamemode == Gamemode::Spectator;
                return Living{.uuid = player->uuid(),
                              .alive = !player->dead,
                              .player = true,
                              .validPlayer = !exempt && !peaceful && !player->dead};
            }
        }
        const ServerEntity* other = world.state().entity(id);
        if (other == nullptr) {
            return std::nullopt;
        }
        return Living{.uuid = other->uuid(), .alive = !other->removed && !other->dying && other->health > 0.0};
    };
    const std::optional<wire::common::Uuid> persistent = anger.target;
    const std::optional<Living> previous = mob.targetId.has_value() ? resolve(*mob.targetId) : std::nullopt;
    if (previous.has_value() && !previous->alive && persistent == previous->uuid && !previous->player) {
        neutral::stop(mob, anger);
        return;
    }
    const bool hasTarget = previous.has_value() && previous->alive;
    if (hasTarget) {
        const bool fresh = !persistent.has_value() || *persistent != previous->uuid;
        if (fresh) {
            anger.target = previous->uuid;
        }
        if (fresh || stayAngry) {
            neutral::start(anger, world.gameTime(), random, minTicks, maxTicks);
        }
    }
    if (persistent.has_value() && !neutral::isAngry(anger, world.gameTime()) &&
        (!hasTarget || !previous->validPlayer || !stayAngry)) {
        neutral::stop(mob, anger);
    }
    if (persistent.has_value()) {
        for (PlayerSession* player : players_->snapshot()) {
            if (player->uuid() == *persistent && player->worldName == world.name() &&
                (peaceful || player->gamemode == Gamemode::Creative || player->gamemode == Gamemode::Spectator)) {
                neutral::stop(mob, anger);
            }
        }
    }
}

void MobActionSystem::tickNeutralMob(world::LoadedWorld& world, ServerEntity& mob, random::RandomSource& random) {
    neutral::Anger& anger = neutral::stateOf(mob);
    if (anger.legacyTime.has_value()) {
        anger.endTime = world.gameTime() + *std::exchange(anger.legacyTime, std::nullopt);
    }
    updateNeutralAnger(world, mob, anger, random, true, neutral::kAngerMinTicks, neutral::kAngerMaxTicks);
    if (mob.type().path() != "enderman") {
        return;
    }
    // parity: `Enderman.setTarget`.
    auto& speed = mob.attributeModifiers["minecraft:movement_speed"];
    if (mob.targetId.has_value()) {
        speed[std::string{kAttackingModifier}] =
            entity::AttributeModifier{.id = std::string{kAttackingModifier}, .amount = kEndermanAttackingSpeed};
    } else {
        speed.erase(std::string{kAttackingModifier});
        if (anger.hadTarget) {
            anger.staredAt = false;
        }
    }
    anger.hadTarget = mob.targetId.has_value();
}

void MobActionSystem::forgiveDeadPlayers(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers) {
    for (PlayerSession* player : watchers) {
        if (!player->dead) {
            mourned_.erase(player->entityId());
            continue;
        }
        if (!mourned_.insert(player->entityId()).second ||
            !world.gameRules().get(world::gameRules::kForgiveDeadPlayers)) {
            continue;
        }
        const double x = std::floor(player->x);
        const double y = std::floor(player->y);
        const double z = std::floor(player->z);
        for (ServerEntity* mob : world.state().entities()) {
            if (mob->removed || !neutral::isNeutral(mob->type())) {
                continue;
            }
            const double half = mob->width() / 2.0;
            if (mob->x + half <= x - kForgiveRange || mob->x - half >= x + 1.0 + kForgiveRange ||
                mob->y + mob->height() <= y - kForgiveRangeY || mob->y >= y + 1.0 + kForgiveRangeY ||
                mob->z + half <= z - kForgiveRange || mob->z - half >= z + 1.0 + kForgiveRange) {
                continue;
            }
            neutral::playerDied(*mob, player->uuid());
        }
    }
}

void MobActionSystem::tickZombifiedPiglin(world::LoadedWorld& world, ServerEntity& mob, random::RandomSource& random) {
    neutral::Anger& anger = neutral::stateOf(mob);
    const std::int64_t now = world.gameTime();
    if (anger.legacyTime.has_value()) {
        anger.endTime = now + *std::exchange(anger.legacyTime, std::nullopt);
    }
    // parity: `setTarget`.
    if (mob.targetId.has_value() && !anger.hadTarget) {
        anger.firstSoundIn = random.nextInt(kFirstSoundMax + 1);
        anger.nextAlertIn = kAlertMin + random.nextInt(kAlertMax - kAlertMin + 1);
    }
    anger.hadTarget = mob.targetId.has_value();
    if (mob.noAi) {
        return;
    }
    auto& speed = mob.attributeModifiers["minecraft:movement_speed"];
    if (neutral::isAngry(anger, now)) {
        if (mob.age >= 0) {
            speed[std::string{kAttackingModifier}] =
                entity::AttributeModifier{.id = std::string{kAttackingModifier}, .amount = 0.05};
        }
        if (anger.firstSoundIn > 0 && --anger.firstSoundIn == 0) {
            random::RandomSource voice = freshRandom();
            const float pitch = ((voice.nextFloat() - voice.nextFloat()) * 0.2F) + (mob.age < 0 ? 1.5F : 1.0F);
            playSound(world, mob, "entity.zombified_piglin.angry", 2.0F, pitch * 1.8F);
        }
    } else {
        speed.erase(std::string{kAttackingModifier});
    }
    updateNeutralAnger(world, mob, anger, random, true, kAngerMin, kAngerMax);
    if (!mob.targetId.has_value()) {
        return;
    }
    // parity: `maybeAlertOthers`, every four to six seconds.
    if (anger.nextAlertIn > 0) {
        --anger.nextAlertIn;
        return;
    }
    anger.nextAlertIn = kAlertMin + random.nextInt(kAlertMax - kAlertMin + 1);
    const double within = mob.attributeWithModifiers("minecraft:follow_range");
    for (ServerEntity* other : world.state().entities()) {
        if (other == &mob || other->removed || other->dying || other->type() != mob.type() ||
            other->targetId.has_value() || other->x + 0.3 < mob.x - within || other->x - 0.3 > mob.x + 1.0 + within ||
            other->y + other->height() < mob.y - kAlertRangeY || other->y > mob.y + 1.0 + kAlertRangeY ||
            other->z + 0.3 < mob.z - within || other->z - 0.3 > mob.z + 1.0 + within) {
            continue;
        }
        other->targetId = mob.targetId;
    }
}

} // namespace basalt::server::runtime
