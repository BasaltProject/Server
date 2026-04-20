#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

struct PlayerView;

namespace endermanGaze {

/// parity: `Enderman.isBeingStaredBy`.
[[nodiscard]] bool staredAtBy(MobContext& mob, const PlayerView& player);

/// parity: `Enderman.teleportTowards`.
bool teleportTowards(MobContext& mob, double x, double eyeY, double z);

} // namespace endermanGaze

/// parity: `Enderman.EndermanLookForPlayerGoal`.
class EndermanLookForPlayerGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] bool angerInducing(MobContext& mob, const PlayerView& player);

    std::optional<std::int32_t> pending_;
    std::optional<std::int32_t> target_;
    std::int32_t aggroTime_ = 0;
    std::int32_t teleportTime_ = 0;
};

/// parity: `Enderman.EndermanFreezeWhenLookedAt`.
class EndermanFreezeWhenLookedAtGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Jump, GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::optional<std::int32_t> target_;
};

} // namespace basalt::server::runtime::entity::ai
