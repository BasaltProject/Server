#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

struct PlayerView;

/// The enderman's gaze: who is staring at it, and what it does about them.
namespace endermanGaze {

/// parity: `Enderman.isBeingStaredBy`: no disguise on the head, and `isLookingAtMe` at its eyes
/// through a cone of 0.025 over the distance, with nothing solid in between.
[[nodiscard]] bool staredAtBy(MobContext& mob, const PlayerView& player);

/// parity: `Enderman.teleportTowards`, sixteen blocks back along the line to it and up to four
/// aside. Answers whether it moved.
bool teleportTowards(MobContext& mob, double x, double eyeY, double z);

} // namespace endermanGaze

/// parity: `Enderman.EndermanLookForPlayerGoal`: somebody staring at it, or somebody it is angry at,
/// becomes its target five ticks on; after that it teleports away from a stare up close and towards
/// a target far off.
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

/// parity: `Enderman.EndermanFreezeWhenLookedAt`: stands still, watching the target staring at it.
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
