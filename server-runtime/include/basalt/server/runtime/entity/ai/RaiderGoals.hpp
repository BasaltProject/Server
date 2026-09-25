#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `Raider.ObtainRaidLeaderBannerGoal`: a raider whose wave lost its captain walks to a
/// dropped ominous banner and puts it on.
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

/// parity: `PathfindToRaidGoal`: out of the village, walk towards its centre and pull idle raiders
/// along into the raid.
class PathfindToRaidGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    std::int32_t recruitmentTick_ = 0;
};

/// parity: `Raider.RaiderMoveThroughVillageGoal`: from house to house, three remembered at a time.
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

/// parity: `Raider.RaiderCelebration`: a raid the village lost, cheered with sounds and hops.
class RaiderCelebrationGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

} // namespace basalt::server::runtime::entity::ai
