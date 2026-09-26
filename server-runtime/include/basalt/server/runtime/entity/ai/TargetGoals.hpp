#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {

/// parity: `NearestAttackableTargetGoal`: one roll in ten before it even looks, the nearest thing
/// inside the mob's own follow range, and it keeps it until it dies or gets away.
class NearestAttackablePlayerGoal final : public Goal {
public:
    static constexpr std::int32_t kDefaultInterval = 10;

    /// `needsDarkness` is `Spider.SpiderTargetGoal`, which only notices anybody in the dark.
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

/// parity: `NearestAttackableTargetGoal` again, for the kinds of mob that go after other mobs: a
/// zombie after a villager, an iron golem after whatever is hostile.
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

/// parity: `HurtByTargetGoal`: whoever hit it becomes what it is after, and it stays after them for
/// as long as they are in range.
class HurtByTargetGoal final : public Goal {
public:
    using Types = std::set<std::string, std::less<>>;

    /// parity: the types whose hits it ignores, and `setAlertOthers` with the types it leaves out.
    explicit HurtByTargetGoal(Types ignoreDamageFrom = {}, bool alertOthers = false, Types ignoreAlert = {});

    /// parity: `BeeHurtByOtherGoal.alertOther`, which only calls the others while it sees the
    /// attacker.
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

/// parity: `MeleeAttackGoal`: walk the target down, repath every four to ten ticks, and swing once a
/// second when both boxes touch.
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

/// What a warden goes after: whoever it is angriest at, and nobody else.
///
/// parity: `WardenAi`'s anger, not a target goal at all in vanilla. The point is the same either
/// way: a warden that has not heard you does not know you are there, however close you stand.
/// parity: the warden's emerge, dig, roar and sonic boom, which hold its other behaviours still.
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

    /// Whoever it is angriest at can change while it is already after somebody.
    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
