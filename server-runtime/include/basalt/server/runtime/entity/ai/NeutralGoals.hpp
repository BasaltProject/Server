#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

struct PlayerView;

/// parity: `NeutralMob.isAngryAt` for a player, `canAttack` being `PlayerView.targetable`.
[[nodiscard]] bool angryAtPlayer(MobContext& mob, const PlayerView& player);

/// The nearest player within the follow range the mob is angry at, or nothing.
[[nodiscard]] const PlayerView* nearestAngeringPlayer(MobContext& mob);

/// parity: `NearestAttackableTargetGoal<Player>(10, true, false, this::isAngryAt)`: the nearest
/// player a neutral mob is angry at, looked for one tick in five.
class AngryAtPlayerGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    std::optional<std::int32_t> found_;
};

/// parity: `ResetUniversalAngerTargetGoal`: under `universal_anger`, a hit from a player drops the
/// grudge for anger at everybody, and with `alertOthers` the same kind around does too.
class ResetUniversalAngerTargetGoal final : public Goal {
public:
    explicit ResetUniversalAngerTargetGoal(bool alertOthers) : alertOthers_(alertOthers) {}

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

private:
    bool alertOthers_;
    std::int32_t lastHurtByPlayerTimestamp_ = 0;
};

} // namespace basalt::server::runtime::entity::ai
