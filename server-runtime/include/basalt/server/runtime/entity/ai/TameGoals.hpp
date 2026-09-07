#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

/// Whoever tamed this mob, if they are here.
[[nodiscard]] const PlayerView* ownerOf(const MobContext& mob);

/// parity: `SitWhenOrderedToGoal`: a pet told to sit stays sat, and nothing else it wants to do
/// outranks that.
class SitWhenOrderedToGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Jump, GoalFlag::Move}; }

    [[nodiscard]] bool isInterruptable() const override { return false; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;
};

/// parity: `FollowOwnerGoal`: a pet that is not sitting keeps within a couple of blocks of whoever
/// tamed it, and starts following again once they are ten blocks away.
class FollowOwnerGoal final : public Goal {
public:
    static constexpr double kStartDistance = 10.0;
    static constexpr double kStopDistance = 2.0;

    explicit FollowOwnerGoal(double speedModifier, double startDistance = kStartDistance,
                             double stopDistance = kStopDistance);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    double startDistance_;
    double stopDistance_;
    std::int32_t repathCooldown_ = 0;
};

/// parity: `BegGoal`: a wolf sits up at whoever is holding something it wants.
class BegGoal final : public Goal {
public:
    BegGoal(double lookDistance, std::set<std::string, std::less<>> items);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double lookDistance_;
    std::set<std::string, std::less<>> items_;
    std::optional<std::int32_t> player_;
    std::int32_t begTime_ = 0;
};

/// parity: `OwnerHurtByTargetGoal`: whoever hit the owner is what the pet goes for.
class OwnerHurtByTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

/// parity: `OwnerHurtTargetGoal`: whatever the owner hit is what the pet goes for.
class OwnerHurtTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
