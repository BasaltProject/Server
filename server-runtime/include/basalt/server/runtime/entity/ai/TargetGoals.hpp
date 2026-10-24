#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

/// parity: `NearestAttackableTargetGoal`.
class NearestAttackablePlayerGoal final : public Goal {
public:
    static constexpr std::int32_t kDefaultInterval = 10;

    explicit NearestAttackablePlayerGoal(std::int32_t randomInterval = kDefaultInterval, bool needsDarkness = false);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::int32_t randomInterval_;
    bool needsDarkness_;
};

/// parity: `NearestAttackableTargetGoal`.
class NearestAttackableEntityGoal final : public Goal {
public:
    static constexpr std::int32_t kDefaultInterval = 10;

    explicit NearestAttackableEntityGoal(std::set<std::string, std::less<>> targets,
                                         std::int32_t randomInterval = kDefaultInterval);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::set<std::string, std::less<>> targets_;
    std::int32_t randomInterval_;
};

/// parity: `Vindicator.VindicatorJohnnyAttackGoal`, which goes for anything alive once the vindicator is Johnny.
class JohnnyAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;
};

/// parity: `HurtByTargetGoal`.
class HurtByTargetGoal final : public Goal {
public:
    using Types = std::set<std::string, std::less<>>;

    /// parity: the types whose hits it ignores, and `setAlertOthers` with the types it leaves out.
    explicit HurtByTargetGoal(Types ignoreDamageFrom = {}, bool alertOthers = false, Types ignoreAlert = {});

    /// parity: `BeeHurtByOtherGoal.alertOther`.
    void alertOnlyInSight() { alertInSight_ = true; }

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    Types ignoreDamageFrom_;
    bool alertOthers_;
    Types ignoreAlert_;
    bool alertInSight_ = false;
};

/// parity: `MeleeAttackGoal`.
class MeleeAttackGoal final : public Goal {
public:
    explicit MeleeAttackGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] static bool withinReach(const MobContext& mob, const TargetView& target);

    double speedModifier_;
    std::int32_t repathCooldown_ = 0;
};

/// parity: `WardenAi`'s anger, not a target goal at all in vanilla.
class WardenBusyGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look, GoalFlag::Jump}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    void start(MobContext& mob) override;
};

class AngerTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
