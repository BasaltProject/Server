#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// parity: `LlamaFollowCaravanGoal`.
class LlamaFollowCaravanGoal final : public Goal {
public:
    explicit LlamaFollowCaravanGoal(double speedModifier);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    double speedModifier_;
    std::int32_t distCheckCounter_ = 0;
};

/// parity: `RangedAttackGoal` as `Llama.registerGoals` builds it.
class LlamaSpitGoal final : public Goal {
public:
    LlamaSpitGoal(double speedModifier, std::int32_t attackIntervalMin, std::int32_t attackIntervalMax,
                  float attackRadius);

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
    float attackRadius_;
    std::optional<std::int32_t> target_;
    std::int32_t seeTime_ = 0;
    std::int32_t attackTime_ = -1;
};

/// parity: `Llama.LlamaHurtByTargetGoal`: revenge that ends with the first spit.
class LlamaHurtByTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override { return revenge_.canUse(mob); }

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override { revenge_.start(mob); }

    void stop(MobContext& mob) override { revenge_.stop(mob); }

private:
    HurtByTargetGoal revenge_;
};

/// parity: `Llama.LlamaAttackWolfGoal`.
class LlamaAttackWolfGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::optional<std::int32_t> target_;
};

/// parity: `TraderLlama.TraderLlamaDefendWanderingTraderGoal`.
class TraderLlamaDefendTraderGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::optional<std::int32_t> ownerLastHurtBy_;
    std::int32_t timestamp_ = 0;
};

/// parity: `Wolf.WolfAvoidEntityGoal`.
class WolfAvoidLlamaGoal final : public Goal {
public:
    WolfAvoidLlamaGoal();

    [[nodiscard]] std::set<GoalFlag> flags() const override { return avoid_.flags(); }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override { return avoid_.canContinueToUse(mob); }

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override { avoid_.stop(mob); }

    void tick(MobContext& mob) override;

private:
    AvoidEntityGoal avoid_;
};

} // namespace basalt::server::runtime::entity::ai
