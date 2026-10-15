#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `RangedCrossbowAttackGoal`.
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

/// parity: `Witch.performRangedAttack`.
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

/// parity: `SpellcasterIllager`.
class CastSpellGoal final : public Goal {
public:
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

} // namespace basalt::server::runtime::entity::ai
