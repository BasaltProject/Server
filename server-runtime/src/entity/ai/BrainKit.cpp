#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::ai::brainKit {
namespace {

namespace mth = wire::common::mth;

/// parity: the player's standing eye height.
constexpr double kPlayerEyes = 1.62;

[[nodiscard]] Block blockOf(const Vec& at) { return {mth::floor(at[0]), mth::floor(at[1]), mth::floor(at[2])}; }

} // namespace

void walkTo(Motion& motion, const Vec& at, float speed, std::int32_t closeEnough, std::optional<std::int32_t> entity) {
    motion.walk = WalkTarget{.at = at, .entity = entity, .speed = speed, .closeEnough = closeEnough};
}

void moveToTarget(MobContext& mob, Motion& motion) {
    if (!motion.walk.has_value()) {
        return;
    }
    if (motion.walk->entity.has_value()) {
        const std::optional<Vec> now = positionOf(mob, *motion.walk->entity, false);
        if (!now.has_value()) {
            motion.walk.reset();
            motion.pathTarget.reset();
            navigation::stop(mob.mob);
            return;
        }
        motion.walk->at = *now;
    }
    const Block target = blockOf(motion.walk->at);
    const Block own{mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)};
    const std::int32_t manhattan =
        std::abs(target[0] - own[0]) + std::abs(target[1] - own[1]) + std::abs(target[2] - own[2]);
    if (manhattan <= motion.walk->closeEnough) {
        motion.walk.reset();
        motion.pathTarget.reset();
        motion.cantReachSince.reset();
        navigation::stop(mob.mob);
        return;
    }
    const bool moved = !motion.pathTarget.has_value() || std::pow(target[0] - (*motion.pathTarget)[0], 2) +
                                                                 std::pow(target[1] - (*motion.pathTarget)[1], 2) +
                                                                 std::pow(target[2] - (*motion.pathTarget)[2], 2) >
                                                             4.0;
    if (!moved && navigation::isDone(mob.mob)) {
        // parity: the run ends with the path, and the walk target goes with it.
        motion.walk.reset();
        motion.pathTarget.reset();
        return;
    }
    if (moved) {
        if (!navigation::moveTo(mob.world, mob.mob, motion.walk->at[0], motion.walk->at[1], motion.walk->at[2],
                                motion.walk->speed)) {
            if (!motion.cantReachSince.has_value()) {
                motion.cantReachSince = mob.gameTime;
            }
            motion.walk.reset();
            motion.pathTarget.reset();
            return;
        }
        motion.cantReachSince.reset();
        motion.pathTarget = target;
    }
}

void lookAtTarget(MobContext& mob, Look& look) {
    if (!look.entity.has_value() && !look.at.has_value()) {
        return;
    }
    if (look.ticksLeft <= 0) {
        // parity: the sink's own run, forty-five to ninety ticks.
        look.ticksLeft = 45 + mob.random.nextInt(46);
    }
    std::optional<Vec> at = look.at;
    if (look.entity.has_value()) {
        at = positionOf(mob, *look.entity, true);
    }
    if (!at.has_value() || --look.ticksLeft <= 0) {
        look = {};
        return;
    }
    lookControl::lookAt(mob.mob, (*at)[0], (*at)[1], (*at)[2]);
}

std::optional<Vec> positionOf(const MobContext& mob, std::int32_t id, bool eyes) {
    if (const PlayerView* player = mob.playerById(id); player != nullptr) {
        return Vec{player->x, player->y + (eyes ? kPlayerEyes : 0.0), player->z};
    }
    const ServerEntity* other = mob.entityById(id);
    if (other == nullptr || other->removed) {
        return std::nullopt;
    }
    return Vec{other->x, other->y + (eyes ? mobBreathing::eyeHeightOf(*other) : 0.0), other->z};
}

std::optional<Vec> strollSpot(MobContext& mob, std::int32_t horizontal, std::int32_t vertical) {
    return randomPos::landPos(mob.world, mob.mob, mob.random, horizontal, vertical);
}

std::optional<Vec> swimSpot(MobContext& mob) {
    const auto fluidAt = [&mob](const Vec& at, bool water) {
        const std::optional<std::int32_t> state =
            mob.world.blockState(mth::floor(at[0]), mth::floor(at[1]), mth::floor(at[2]));
        const world::Submersion fluid = state.has_value() ? world::fluids::of(*state) : world::Submersion::None;
        return water ? fluid == world::Submersion::Water : fluid != world::Submersion::None;
    };
    // parity: `BehaviorUtils.getRandomSwimmablePos`: ten more tries for a spot in water.
    const auto swimmable = [&](std::int32_t horizontal, std::int32_t vertical) {
        std::optional<Vec> at = randomPos::defaultPos(mob.world, mob.mob, mob.random, horizontal, vertical);
        for (std::int32_t count = 0; at.has_value() && !fluidAt(*at, true) && count++ < 10;) {
            at = randomPos::defaultPos(mob.world, mob.mob, mob.random, horizontal, vertical);
        }
        return at;
    };
    static constexpr std::array<std::array<std::int32_t, 2>, 6> kTiers{
        {{1, 1}, {3, 3}, {5, 5}, {6, 5}, {7, 7}, {10, 7}}};
    std::optional<Vec> fallback;
    std::optional<Vec> target;
    for (const auto& [horizontal, vertical] : kTiers) {
        if (!fallback.has_value()) {
            target = swimmable(horizontal, vertical);
        } else {
            // parity: past the first find, the same way out, only further.
            const Vec to{(*fallback)[0] - mob.mob.x, (*fallback)[1] - mob.mob.y, (*fallback)[2] - mob.mob.z};
            const double length = std::sqrt((to[0] * to[0]) + (to[1] * to[1]) + (to[2] * to[2]));
            const Vec unit =
                length < static_cast<double>(1.0E-5F) ? Vec{} : Vec{to[0] / length, to[1] / length, to[2] / length};
            target = Vec{mob.mob.x + (unit[0] * horizontal), mob.mob.y + (unit[1] * vertical),
                         mob.mob.z + (unit[2] * horizontal)};
        }
        if (!target.has_value() || !fluidAt(*target, false)) {
            return fallback;
        }
        fallback = target;
    }
    return target;
}

std::optional<Block> nearestBlock(const MobContext& mob, std::int32_t rangeXZ, std::int32_t rangeY,
                                  const std::function<bool(std::int32_t stateId)>& matches) {
    return nearestBlockWhere(mob, rangeXZ, rangeY,
                             [&matches](const Block& /*at*/, std::int32_t stateId) { return matches(stateId); });
}

std::optional<Block> nearestBlockWhere(const MobContext& mob, std::int32_t rangeXZ, std::int32_t rangeY,
                                       const std::function<bool(const Block& at, std::int32_t stateId)>& matches) {
    const Block own{mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)};
    const std::int32_t maxDepth = rangeXZ + rangeY + rangeXZ;
    for (std::int32_t depth = 0; depth <= maxDepth; ++depth) {
        const std::int32_t maxX = std::min(rangeXZ, depth);
        for (std::int32_t dx = -maxX; dx <= maxX; ++dx) {
            const std::int32_t restX = depth - std::abs(dx);
            const std::int32_t maxY = std::min(rangeY, restX);
            for (std::int32_t dy = -maxY; dy <= maxY; ++dy) {
                const std::int32_t dz = restX - std::abs(dy);
                if (dz > rangeXZ) {
                    continue;
                }
                for (const std::int32_t sign : {1, -1}) {
                    if (sign < 0 && dz == 0) {
                        break;
                    }
                    const Block at{own[0] + dx, own[1] + dy, own[2] + (sign * dz)};
                    const std::optional<std::int32_t> state = mob.world.blockState(at[0], at[1], at[2]);
                    if (state.has_value() && matches(at, *state)) {
                        return at;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> shuffled(random::RandomSource& random, const std::vector<std::int32_t>& weights) {
    std::vector<std::pair<double, std::size_t>> drawn;
    drawn.reserve(weights.size());
    for (std::size_t index = 0; index < weights.size(); ++index) {
        drawn.emplace_back(-std::pow(static_cast<double>(random.nextFloat()), 1.0 / weights[index]), index);
    }
    std::ranges::stable_sort(drawn, {}, &std::pair<double, std::size_t>::first);
    std::vector<std::size_t> order;
    order.reserve(drawn.size());
    for (const auto& entry : drawn) {
        order.push_back(entry.second);
    }
    return order;
}

std::vector<Seen> nearby(const MobContext& mob, double range) {
    std::vector<Seen> seen;
    const auto inBox = [&](double x, double y, double z) {
        return std::abs(x - mob.mob.x) <= range + (mob.mob.width() / 2.0) &&
               std::abs(z - mob.mob.z) <= range + (mob.mob.width() / 2.0) &&
               y <= mob.mob.y + mob.mob.height() + range && y >= mob.mob.y - range - 2.0;
    };
    for (ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->dying || !inBox(other->x, other->y, other->z)) {
            continue;
        }
        seen.push_back(Seen{.mob = other,
                            .id = other->entityId(),
                            .distanceSquared = mob.distanceSquaredTo(other->x, other->y, other->z),
                            .type = other->type().path()});
    }
    for (const PlayerView& player : mob.players) {
        if (player.spectator || !inBox(player.x, player.y, player.z)) {
            continue;
        }
        seen.push_back(Seen{.player = &player,
                            .id = player.entityId,
                            .distanceSquared = mob.distanceSquaredTo(player.x, player.y, player.z),
                            .type = "player"});
    }
    std::ranges::stable_sort(seen, {}, &Seen::distanceSquared);
    return seen;
}

bool withinMeleeRange(const MobContext& mob, const TargetView& target) {
    const double reach = (mob.mob.width() / 2.0) + mobCombat::attackReach() + (target.width / 2.0);
    if (std::abs(target.x - mob.mob.x) > reach || std::abs(target.z - mob.mob.z) > reach) {
        return false;
    }
    return target.y <= mob.mob.y + mob.mob.height() && target.y + target.height >= mob.mob.y;
}

bool meleeAttack(MobContext& mob, std::int64_t& coolingUntil, std::int32_t cooldown) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value() || mob.gameTime < coolingUntil || !withinMeleeRange(mob, *target)) {
        return false;
    }
    coolingUntil = mob.gameTime + cooldown;
    mob.actions.swing(mob.mob);
    const float damage = entityCombat::attackDamageOf(mob.mob);
    if (target->playerId.has_value()) {
        mob.actions.hurtPlayer(mob.mob, *target->playerId, damage);
    } else if (target->entity != nullptr) {
        mob.actions.hurtMob(mob.mob, *target->entity, damage);
    }
    return true;
}

void chaseTarget(MobContext& mob, Motion& motion, Look& look, float speed) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    look = Look{.entity = mob.mob.targetId};
    if (withinMeleeRange(mob, *target)) {
        motion.walk.reset();
        return;
    }
    walkTo(motion, {target->x, target->y, target->z}, speed, 0, mob.mob.targetId);
}

std::optional<std::int32_t> makeLove(MobContext& mob, Motion& motion, Look& look, float speed,
                                     std::int32_t closeEnough) {
    if (mob.mob.loveTicks <= 0 || mob.mob.age != 0) {
        return std::nullopt;
    }
    ServerEntity* partner = nullptr;
    double best = 8.0 * 8.0;
    for (ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->dying || other->type() != mob.mob.type() ||
            other->loveTicks <= 0 || other->age != 0) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(other->x, other->y, other->z);
        if (distance < best) {
            best = distance;
            partner = other;
        }
    }
    if (partner == nullptr) {
        return std::nullopt;
    }
    look = Look{.entity = partner->entityId()};
    walkTo(motion, {partner->x, partner->y, partner->z}, speed, closeEnough, partner->entityId());
    return partner->entityId();
}

bool followAdult(MobContext& mob, Motion& motion, Look& look, std::int32_t minRange, std::int32_t maxRange,
                 float speed) {
    if (mob.mob.age >= 0 || motion.walk.has_value()) {
        return false;
    }
    const ServerEntity* adult = nullptr;
    double best = 16.0 * 16.0;
    for (const ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->type() != mob.mob.type() || other->age < 0 || other->removed) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(other->x, other->y, other->z);
        if (distance < best) {
            best = distance;
            adult = other;
        }
    }
    if (adult == nullptr || best >= (maxRange + 1.0) * (maxRange + 1.0) || best < minRange * minRange) {
        return false;
    }
    look = Look{.entity = adult->entityId()};
    walkTo(motion, {adult->x, adult->y, adult->z}, speed, minRange - 1, adult->entityId());
    return true;
}

void lookSometimes(MobContext& mob, Look& look, std::int32_t& ticker, double range, std::int32_t min, std::int32_t max,
                   std::string_view type) {
    if (look.entity.has_value() || look.at.has_value()) {
        return;
    }
    const std::vector<Seen> seen = nearby(mob, range);
    const auto found = std::ranges::find_if(seen, [&](const Seen& one) {
        return (type.empty() || one.type == type) && one.distanceSquared <= range * range;
    });
    if (found == seen.end()) {
        return;
    }
    // parity: `Ticker.tickDownAndCheck`: a look when it reaches nothing, then a new wait.
    if (ticker > 0) {
        --ticker;
        return;
    }
    ticker = min + mob.random.nextInt(max - min + 1);
    look = Look{.entity = found->id};
}

void idleMovement(MobContext& mob, Motion& motion, const Look& look, std::int32_t& idleTicks, float speed) {
    if (idleTicks > 0) {
        --idleTicks;
        return;
    }
    for (const std::size_t choice : shuffled(mob.random, {2, 2, 1})) {
        if (choice == 2) {
            idleTicks = 30 + mob.random.nextInt(31);
            return;
        }
        if (motion.walk.has_value()) {
            continue;
        }
        if (choice == 0) {
            if (const std::optional<Vec> spot = strollSpot(mob); spot.has_value()) {
                walkTo(motion, *spot, speed, 0);
                return;
            }
            continue;
        }
        std::optional<Vec> at = look.at;
        if (look.entity.has_value()) {
            at = positionOf(mob, *look.entity, false);
        }
        if (at.has_value()) {
            walkTo(motion, *at, speed, 3);
            return;
        }
    }
}

void lookAround(MobContext& mob, Look& look, std::int32_t& gazeCooldown, std::int32_t min, std::int32_t max,
                float maxYaw) {
    if (look.entity.has_value() || look.at.has_value() || gazeCooldown > 0) {
        return;
    }
    constexpr float kRadians = static_cast<float>(std::numbers::pi) / 180.0F;
    constexpr auto kPi = static_cast<float>(std::numbers::pi);
    // parity: the pitch is drawn even though its range is nothing.
    const float pitch = std::clamp((mob.random.nextFloat() * 0.0F) + 0.0F, -90.0F, 90.0F);
    float yaw = std::fmod(mob.mob.yaw + (2.0F * mob.random.nextFloat() * maxYaw) - maxYaw, 360.0F);
    if (yaw >= 180.0F) {
        yaw -= 360.0F;
    }
    if (yaw < -180.0F) {
        yaw += 360.0F;
    }
    // parity: `Vec3.directionFromRotation` off the eyes, and a `BlockPosTracker` of that square.
    const float yCos = mth::cos(static_cast<double>((-yaw * kRadians) - kPi));
    const float ySin = mth::sin(static_cast<double>((-yaw * kRadians) - kPi));
    const float xCos = -mth::cos(static_cast<double>(-pitch * kRadians));
    const float xSin = mth::sin(static_cast<double>(-pitch * kRadians));
    const double eyes = mob.mob.y + mobBreathing::eyeHeightOf(mob.mob);
    const Block at{mth::floor(mob.mob.x + static_cast<double>(ySin * xCos)), mth::floor(eyes + xSin),
                   mth::floor(mob.mob.z + static_cast<double>(yCos * xCos))};
    look = Look{.at = Vec{at[0] + 0.5, at[1] + 0.5, at[2] + 0.5}};
    gazeCooldown = min + mob.random.nextInt(max - min + 1);
}

bool panic(MobContext& mob, Motion& motion, std::int32_t& panicTicks, float speed) {
    if (panicTicks == 0 && mob.mob.panicTicks > 0) {
        panicTicks = 100 + mob.random.nextInt(21);
        motion.walk.reset();
        navigation::stop(mob.mob);
    }
    if (panicTicks <= 0) {
        return false;
    }
    --panicTicks;
    if (navigation::isDone(mob.mob)) {
        if (const std::optional<Vec> spot = strollSpot(mob, 5, 4); spot.has_value()) {
            walkTo(motion, *spot, speed, 0);
        }
    }
    return true;
}

bool tempt(MobContext& mob, Motion& motion, Look& look, Temptation& temptation, float speed,
           const std::function<bool(const wire::common::Identifier&)>& tempts, double closeEnough) {
    const PlayerView* player = nullptr;
    double best = 10.0 * 10.0;
    for (const PlayerView& candidate : mob.players) {
        if (candidate.spectator || !candidate.heldItem.has_value() || !tempts(*candidate.heldItem)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate.x, candidate.y, candidate.z);
        if (distance <= best) {
            best = distance;
            player = &candidate;
        }
    }
    if (temptation.cooldown > 0) {
        --temptation.cooldown;
        return false;
    }
    if (player == nullptr) {
        // parity: `FollowTemptation.stop`: a hundred ticks' rest, and the walk and look let go.
        if (temptation.following) {
            temptation.following = false;
            temptation.cooldown = 100;
            motion.walk.reset();
            look = {};
        }
        return false;
    }
    temptation.following = true;
    look = Look{.entity = player->entityId};
    if (best < closeEnough * closeEnough) {
        motion.walk.reset();
    } else {
        walkTo(motion, {player->x, player->y, player->z}, speed, 2, player->entityId);
    }
    return true;
}

} // namespace basalt::server::runtime::entity::ai::brainKit
