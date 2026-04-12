#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::entity::ai {

class MobContext;
struct PlayerView;
struct TargetView;

/// The pieces every brain-driven mob shares: memories that run out, the walk and look targets
/// `MoveToTargetSink` and `LookAtTargetSink` follow, the idle shuffle and what the sensors see.
///
/// parity: `Brain`'s memories with expiry and the core behaviours most brains start with.
namespace brainKit {

using Vec = std::array<double, 3>;
using Block = std::array<std::int32_t, 3>;

/// parity: a brain memory, optionally set with an expiry in ticks.
template <typename T>
struct Memory {
    std::optional<T> value;
    std::int64_t until = -1;

    [[nodiscard]] bool has(std::int64_t now) const { return value.has_value() && (until < 0 || now < until); }

    [[nodiscard]] const T* get(std::int64_t now) const { return has(now) ? &*value : nullptr; }

    void set(T next) {
        value = std::move(next);
        until = -1;
    }

    void setFor(T next, std::int64_t now, std::int64_t ticks) {
        value = std::move(next);
        until = now + ticks;
    }

    void erase() { value.reset(); }
};

/// parity: `WalkTarget` over a point or an entity it follows.
struct WalkTarget {
    Vec at{};
    std::optional<std::int32_t> entity;
    float speed = 1.0F;
    std::int32_t closeEnough = 0;
};

struct Motion {
    std::optional<WalkTarget> walk;
    std::optional<Block> pathTarget;
    /// parity: `CANT_REACH_WALK_TARGET_SINCE`.
    std::optional<std::int64_t> cantReachSince;
};

/// parity: `MoveToTargetSink`: a path to the walk target, drawn again when it moves two blocks, and
/// the target forgotten once reached or out of reach.
void moveToTarget(MobContext& mob, Motion& motion);

/// parity: `BehaviorUtils.setWalkAndLookTargetMemories`.
void walkTo(Motion& motion, const Vec& at, float speed, std::int32_t closeEnough,
            std::optional<std::int32_t> entity = std::nullopt);

struct Look {
    std::optional<std::int32_t> entity;
    std::optional<Vec> at;
    std::int32_t ticksLeft = 0;
};

/// parity: `LookAtTargetSink(45, 90)`.
void lookAtTarget(MobContext& mob, Look& look);

/// Where an entity or player is, at its eyes when asked. Nothing once it is gone.
[[nodiscard]] std::optional<Vec> positionOf(const MobContext& mob, std::int32_t id, bool eyes);

/// parity: `LandRandomPos.getPos(mob, horizontal, vertical)`, `RandomStroll.stroll`'s spot.
[[nodiscard]] std::optional<Vec> strollSpot(MobContext& mob, std::int32_t horizontal = 10, std::int32_t vertical = 7);

/// parity: `RandomStroll.getTargetSwimPos`: water further out tier by tier, the last good one kept.
[[nodiscard]] std::optional<Vec> swimSpot(MobContext& mob);

/// parity: `findBlocksInBoxByManhattanDistance(pos, rangeXZ, rangeY)`: the first square, nearest
/// shells first, whose state `matches`.
[[nodiscard]] std::optional<Block> nearestBlock(const MobContext& mob, std::int32_t rangeXZ, std::int32_t rangeY,
                                                const std::function<bool(std::int32_t stateId)>& matches);

/// `nearestBlock` with the square's position given too.
[[nodiscard]] std::optional<Block>
nearestBlockWhere(const MobContext& mob, std::int32_t rangeXZ, std::int32_t rangeY,
                  const std::function<bool(const Block& at, std::int32_t stateId)>& matches);

/// parity: `ShufflingList.shuffle`: each weight drawn as `-nextFloat()^(1/weight)`, smallest first.
[[nodiscard]] std::vector<std::size_t> shuffled(random::RandomSource& random, const std::vector<std::int32_t>& weights);

/// One living thing a sensor sees, nearest first.
struct Seen {
    ServerEntity* mob = nullptr;
    const PlayerView* player = nullptr;
    std::int32_t id = 0;
    double distanceSquared = 0.0;
    std::string_view type;
};

/// parity: the sensors' last scan, every twenty ticks from a random start, and the sight test of each
/// thing seen, asked once a tick.
struct SensorScan {
    std::int64_t nextScan = 0;
    bool started = false;
    std::vector<std::pair<std::int32_t, bool>> seen;
    std::int64_t sightTime = -1;
    std::vector<std::pair<std::int32_t, bool>> sight;
};

/// parity: `NearestLivingEntitySensor` and `PlayerSensor`: what the last scan of the follow range
/// saw, in the scan's order, within `range` of where it is now. `visibleOnly` is the
/// `NEAREST_VISIBLE_*` memories, which keep what `Sensor.isEntityTargetable` lets through.
[[nodiscard]] std::vector<Seen> nearby(const MobContext& mob, double range, bool visibleOnly = true);

/// parity: `Mob.isWithinMeleeAttackRange`.
[[nodiscard]] bool withinMeleeRange(const MobContext& mob, const TargetView& target);

/// parity: `MeleeAttack.create(cooldown)`: a swing and a hit once in reach and cooled down.
/// Answers whether it struck.
bool meleeAttack(MobContext& mob, std::int64_t& coolingUntil, std::int32_t cooldown);

/// parity: `SetWalkTargetFromAttackTargetIfTargetOutOfReach`.
void chaseTarget(MobContext& mob, Motion& motion, Look& look, float speed);

/// parity: `AnimalMakeLove`: the nearest mate of its kind in love within eight, walked to. Answers
/// the partner, which is the brain's `BREED_TARGET`.
std::optional<std::int32_t> makeLove(MobContext& mob, Motion& motion, Look& look, float speed,
                                     std::int32_t closeEnough);

/// parity: `BabyFollowAdult`: a baby keeps between `minRange` and `maxRange` of the nearest adult.
bool followAdult(MobContext& mob, Motion& motion, Look& look, std::int32_t minRange, std::int32_t maxRange,
                 float speed);

/// parity: `SetEntityLookTargetSometimes`: the nearest living thing, of `type` when given, every
/// `min` to `max` ticks.
void lookSometimes(MobContext& mob, Look& look, std::int32_t& ticker, double range, std::int32_t min, std::int32_t max,
                   std::string_view type = {});

/// parity: `RunOne` of `RandomStroll.stroll(speed)`, `SetWalkTargetFromLookTarget(speed, 3)` and
/// `DoNothing(30, 60)`, weighted two, two and one.
void idleMovement(MobContext& mob, Motion& motion, const Look& look, std::int32_t& idleTicks, float speed);

/// parity: `RandomLookAround(min..max, maxYaw, 0, 0)`: with no look target and the gaze rested, a
/// glance up to `maxYaw` degrees either side of where the body faces.
void lookAround(MobContext& mob, Look& look, std::int32_t& gazeCooldown, std::int32_t min, std::int32_t max,
                float maxYaw);

/// parity: `AnimalPanic`: a hundred to a hundred and twenty ticks of running after a hit. Answers
/// whether it is panicking.
bool panic(MobContext& mob, Motion& motion, std::int32_t& panicTicks, float speed);

/// parity: `TemptingSensor` and `FollowTemptation`: the nearest player within ten holding what
/// tempts it, followed to `closeEnough`; a hundred ticks' rest after. Answers whether it follows.
struct Temptation {
    std::int32_t cooldown = 0;
    bool following = false;
};

bool tempt(MobContext& mob, Motion& motion, Look& look, Temptation& temptation, float speed,
           const std::function<bool(const wire::common::Identifier&)>& tempts, double closeEnough = 2.5);

} // namespace brainKit

} // namespace basalt::server::runtime::entity::ai
