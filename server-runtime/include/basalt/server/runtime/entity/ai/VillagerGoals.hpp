#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity::ai {

/// The blocks a village is made of, as far as the goals that walk to one are concerned.
namespace villageBlocks {

using Names = std::set<std::string, std::less<>>;

/// parity: `PoiTypes`, minus the professions this build does not hand out.
[[nodiscard]] const Names& workstations();

[[nodiscard]] const Names& bells();

[[nodiscard]] const Names& beds();

/// The nearest block of any of these names. Answers block coordinates, not a middle.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> findNearest(const world::WorldState& world, double fromX,
                                                                     double fromY, double fromZ, const Names& names,
                                                                     std::int32_t range, std::int32_t height);

[[nodiscard]] bool matches(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                           const Names& names);

} // namespace villageBlocks

/// Walks to a block of a given kind and stays by it.
///
/// parity: the `walk_to_target` half of the villager's work, meet and rest packages, which is all
/// those packages do that anybody can see from outside.
class WalkToBlockGoal : public Goal {
public:
    WalkToBlockGoal(double speed, const villageBlocks::Names& names, std::int32_t range, std::int32_t height = 4);

    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move, GoalFlag::Look}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

protected:
    /// Whether this hour, or this raid, is one where the villager wants to be there at all.
    [[nodiscard]] virtual bool wanted(MobContext& /*mob*/) { return true; }

    /// Called the tick the mob reaches the block, and again the tick it gives up on it.
    virtual void onArrived(MobContext& /*mob*/, bool /*arrived*/) {}

private:
    /// The middle of the block itself when it can be stood in, or of whichever side is free.
    [[nodiscard]] std::optional<std::array<double, 2>> standingSpot(MobContext& mob) const;

    double speed_;
    const villageBlocks::Names* names_;
    std::int32_t range_;
    std::int32_t height_;
    std::int32_t targetX_ = 0;
    std::int32_t targetY_ = 0;
    std::int32_t targetZ_ = 0;
    bool found_ = false;
    std::int32_t searchCooldown_ = 0;
    bool arrived_ = false;
    double walkX_ = 0.0;
    double walkZ_ = 0.0;
};

/// parity: the work package, which sends a villager to its own job site for the working hours.
class WorkAtSiteGoal final : public WalkToBlockGoal {
public:
    explicit WorkAtSiteGoal(double speed);

protected:
    [[nodiscard]] bool wanted(MobContext& mob) override;
};

/// parity: the meet package, which gathers the village at the bell in the afternoon.
class MeetAtBellGoal final : public WalkToBlockGoal {
public:
    explicit MeetAtBellGoal(double speed);

protected:
    [[nodiscard]] bool wanted(MobContext& mob) override;
};

/// parity: the rest package, and the hide package it shares its bed with: a villager sleeps at night
/// and runs for the same bed while a raid is on, whatever the hour.
class SleepInBedGoal final : public WalkToBlockGoal {
public:
    explicit SleepInBedGoal(double speed);

protected:
    [[nodiscard]] bool wanted(MobContext& mob) override;

    void onArrived(MobContext& mob, bool arrived) override;
};

} // namespace basalt::server::runtime::entity::ai
