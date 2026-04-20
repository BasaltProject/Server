#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `Raider.ObtainRaidLeaderBannerGoal`.
class ObtainRaidLeaderBannerGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::map<std::int32_t, std::int64_t> unreachable_;
    std::optional<std::int32_t> banner_;
    std::array<double, 3> bannerAt_{};
};

/// parity: `PathfindToRaidGoal`.
class PathfindToRaidGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t recruitmentTick_ = 0;
};

/// parity: `Raider.RaiderMoveThroughVillageGoal`.
class RaiderMoveThroughVillageGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::array<std::int32_t, 3> poi_{};
    std::vector<std::array<std::int32_t, 3>> visited_;
    bool stuck_ = false;
};

/// parity: `Raider.RaiderCelebration`.
class RaiderCelebrationGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
