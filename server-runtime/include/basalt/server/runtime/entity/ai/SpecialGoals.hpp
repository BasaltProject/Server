#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <optional>
#include <set>

namespace basalt::server::runtime::entity::ai {

/// How far apart two points are, for the goals that measure it themselves.
[[nodiscard]] double distanceBetween(double ax, double ay, double az, double bx, double by, double bz);

/// parity: `Spider.onClimbable`, which is simply "am I walking into something": while it is, the
/// climb replaces the fall, which is what takes a spider straight up the side of a house.
class ClimbWallsGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `EnderMan.EndermanLookForPlayerGoal` over `isLookingAtMe`: the player has to be looking
/// almost straight at its eyes, and the cone widens the closer they are.
class StareTriggerGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

private:
    [[nodiscard]] static bool staredAtBy(const MobContext& mob, const PlayerView& player);

    std::int32_t aggroTime_ = 0;
    std::optional<std::int32_t> pending_;
};

/// parity: `EnderMan.teleport`, `teleportTowards` and `customServerAiStep`: it goes away when it is
/// hit, when it is rained on or standing in water, and when the sun catches it holding no target.
class TeleportAwayGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;

private:
    static void teleportRandomly(MobContext& mob);
};

} // namespace basalt::server::runtime::entity::ai
