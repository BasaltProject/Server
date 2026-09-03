#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `RangedCrossbowAttackGoal`: close to inside its range, wind the crossbow for a second and
/// a quarter, let it go, and wait before the next one.
class CrossbowAttackGoal final : public Goal {
public:
    CrossbowAttackGoal(double speedModifier, double attackRadius);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    double attackRadius_;
    std::int32_t charge_ = 0;
    std::int32_t attackDelay_ = 0;
    std::int32_t seeTime_ = 0;
};

/// parity: `Witch.performRangedAttack`: which potion it throws depends on how far away you are and
/// what you already have on you, and it drinks one itself when it is hurt.
class ThrowPotionGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t attackDelay_ = 0;
};

/// parity: `SpellcasterIllager`'s own rhythm: a spell is cast after a wind-up, and then nothing for
/// a while. The evoker's fangs and vexes, and the illusioner's blindness, all hang off this.
class CastSpellGoal final : public Goal {
public:
    /// Which spell this goal casts.
    enum class Spell : std::uint8_t { Fangs, SummonVexes, Blindness };

    CastSpellGoal(Spell spell, std::int32_t castingTime, std::int32_t cooldown);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    Spell spell_;
    std::int32_t castingTime_;
    std::int32_t cooldown_;
    std::int32_t casting_ = 0;
    std::int32_t nextCast_ = 0;
};

/// parity: `PathfindToRaidGoal`: a raider with a raid to be at walks to the village and only stops
/// when it is inside it or has somebody to hit.
class PathfindToRaidGoal final : public Goal {
public:
    explicit PathfindToRaidGoal(double speed);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    void moveToCentre(MobContext& mob) const;

    double speed_;
};

} // namespace basalt::server::runtime::entity::ai
