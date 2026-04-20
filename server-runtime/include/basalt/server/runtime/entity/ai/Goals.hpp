#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

struct TargetView {
    std::optional<std::int32_t> playerId;
    ServerEntity* entity = nullptr;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double width = 0.0;
    double height = 0.0;
};

[[nodiscard]] std::optional<TargetView> targetOf(const MobContext& mob);

/// parity: `FloatGoal`, which is why a mob in water bobs instead of drowning.
class FloatGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Jump}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `RandomStrollGoal`.
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

/// parity: `RandomLookAroundGoal`.
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

/// parity: `PanicGoal`.
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

/// parity: `TemptGoal`.
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

/// parity: `BreedGoal`.
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

/// parity: `EatBlockGoal`.
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

    /// parity: `AvoidEntityGoal.toAvoid`, what the last `canUse` found.
    [[nodiscard]] const ServerEntity* threat() const noexcept { return threat_; }

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

/// parity: `SwellGoal`.
class SwellGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `RangedAttackGoal`.
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

/// parity: `Squid.SquidRandomMovementGoal`.
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

/// parity: `AbstractCubeMob.CubeMobMoveControl`.
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
