#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

/// Whatever a mob is after, whether that is a player or another mob.
struct TargetView {
    std::optional<std::int32_t> playerId;
    ServerEntity* entity = nullptr;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double width = 0.0;
    double height = 0.0;
};

/// The thing this mob is after, resolved against what is in the world this tick.
[[nodiscard]] std::optional<TargetView> targetOf(const MobContext& mob);

/// parity: `FloatGoal`, which is why a mob in water bobs instead of drowning.
class FloatGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Jump}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `RandomStrollGoal` over `DefaultRandomPos.getPos(mob, 10, 7)`: one roll in sixty (the
/// interval halved, since the goal is not ticked every tick), and the walk it starts is a real path.
class RandomStrollGoal : public Goal {
public:
    static constexpr std::int32_t kDefaultInterval = 120;

    explicit RandomStrollGoal(double speedModifier, std::int32_t interval = kDefaultInterval);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    double speedModifier_;
    std::int32_t interval_;
    std::optional<std::array<double, 3>> wanted_;
};

/// parity: `LookAtPlayerGoal`, the one that makes a mob turn its head as you walk past.
class LookAtPlayerGoal final : public Goal {
public:
    static constexpr float kDefaultProbability = 0.02F;

    explicit LookAtPlayerGoal(double lookDistance, float probability = kDefaultProbability);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double lookDistance_;
    float probability_;
    std::optional<std::int32_t> lookAt_;
    std::int32_t lookTime_ = 0;
};

/// parity: `RandomLookAroundGoal`, one roll in fifty a tick and a second of looking that way.
class RandomLookAroundGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double relX_ = 0.0;
    double relZ_ = 0.0;
    std::int32_t lookTime_ = 0;
};

/// parity: `PanicGoal`: something hurt it, so it runs to a spot within five blocks and keeps running
/// until it gets there.
class PanicGoal final : public Goal {
public:
    explicit PanicGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    double speedModifier_;
    std::optional<std::array<double, 3>> wanted_;
};

/// parity: `TemptGoal`: a player holding the right food is followed to within two and a half blocks,
/// and the mob looks at them the whole way.
class TemptGoal final : public Goal {
public:
    static constexpr double kDefaultStopDistance = 2.5;

    TemptGoal(double speedModifier, std::set<std::string, std::less<>> items,
              double stopDistance = kDefaultStopDistance);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    std::set<std::string, std::less<>> items_;
    double stopDistance_;
    std::optional<std::int32_t> player_;
};

/// parity: `BreedGoal`: two of the same kind in love walk together for three seconds and breed.
class BreedGoal final : public Goal {
public:
    explicit BreedGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    ServerEntity* partner_ = nullptr;
    std::int32_t loveTime_ = 0;
};

/// parity: `FollowParentGoal`: a calf walks after the nearest grown one of its own kind.
class FollowParentGoal final : public Goal {
public:
    explicit FollowParentGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    ServerEntity* parent_ = nullptr;
    std::int32_t repathCooldown_ = 0;
};

/// parity: `EatBlockGoal`: a sheep stops, eats the grass under it, and leaves dirt behind.
class EatBlockGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look, GoalFlag::Jump}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t eatTicks_ = 0;
};

/// parity: `AvoidEntityGoal`: a creeper walks away from a cat, a skeleton from a wolf.
class AvoidEntityGoal final : public Goal {
public:
    AvoidEntityGoal(std::set<std::string, std::less<>> avoided, double distance, double walkSpeed, double sprintSpeed);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::set<std::string, std::less<>> avoided_;
    double distance_;
    double walkSpeed_;
    double sprintSpeed_;
    ServerEntity* threat_ = nullptr;
    std::optional<std::array<double, 3>> wanted_;
};

/// parity: `LeapAtTargetGoal`, the pounce a spider or a wolf makes at what it is chasing.
class LeapAtTargetGoal final : public Goal {
public:
    explicit LeapAtTargetGoal(double height);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Jump, GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

private:
    double height_;
};

/// parity: `SwellGoal` over `Creeper.tick`: within three blocks of what it is after the fuse counts
/// up, past seven or out of sight it counts back down, and at thirty it goes off.
class SwellGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `RangedAttackGoal` over `AbstractSkeleton.performRangedAttack`: it closes to inside its
/// range, stands still once it can see you, and lets an arrow go on its own clock.
class RangedAttackGoal final : public Goal {
public:
    RangedAttackGoal(double speedModifier, std::int32_t attackIntervalMin, std::int32_t attackIntervalMax,
                     double attackRadius);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    std::int32_t attackIntervalMin_;
    std::int32_t attackIntervalMax_;
    double attackRadius_;
    std::int32_t seeTime_ = 0;
    std::int32_t attackTime_ = -1;
};

/// parity: `Squid.SquidRandomMovementGoal` with `Squid.aiStep`: a squid does not walk a path at all,
/// it points itself somewhere and pushes, and the water slows it down between pushes.
class SwimAboutGoal final : public Goal {
public:
    /// parity: the 0.2 a squid pushes with.
    static constexpr double kSquidPush = 0.2;

    explicit SwimAboutGoal(double push = kSquidPush);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;

private:
    double push_;
    double vectorX_ = 0.0;
    double vectorY_ = 0.0;
    double vectorZ_ = 0.0;
};

/// parity: `AbstractCubeMob.CubeMobMoveControl`: a slime does not walk, it turns to a chosen heading
/// and hops, with ten to thirty ticks between hops and a third of that when it is angry.
class HopAboutGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Jump, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;

private:
    float heading_ = 0.0F;
    std::int32_t nextTurn_ = 0;
    std::int32_t jumpDelay_ = 0;
};

} // namespace basalt::server::runtime::entity::ai
