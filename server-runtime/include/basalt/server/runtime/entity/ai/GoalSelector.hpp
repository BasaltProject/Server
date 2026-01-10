#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// One goal with the priority it was registered at. parity: `WrappedGoal`.
class WrappedGoal {
public:
    WrappedGoal(std::int32_t priority, std::unique_ptr<Goal> goal);

    [[nodiscard]] std::int32_t priority() const noexcept { return priority_; }

    [[nodiscard]] Goal& goal() const noexcept { return *goal_; }

    [[nodiscard]] bool running() const noexcept { return running_; }

    [[nodiscard]] bool canBeReplacedBy(const WrappedGoal& other) const;

    void start(MobContext& mob);

    void stop(MobContext& mob);

private:
    std::int32_t priority_;
    std::unique_ptr<Goal> goal_;
    bool running_ = false;
};

/// What decides, every tick, which of a mob's goals are running.
///
/// parity: `GoalSelector.tick`, in its own order: stop what can no longer continue, free the flags
/// it held, start whatever can start and outranks what holds its flags, then tick everything
/// running.
class GoalSelector {
public:
    void add(std::int32_t priority, std::unique_ptr<Goal> goal);

    [[nodiscard]] std::size_t size() const noexcept { return goals_.size(); }

    /// Whether a goal of this kind is running, which is all the outside needs to know.
    [[nodiscard]] bool isRunning(const std::function<bool(const Goal&)>& predicate) const;

    void tick(MobContext& mob);

    /// parity: `GoalSelector.tickRunningGoals`, which is all a mob does on the ticks between full
    /// decisions: `Mob` alternates by `tickCount + id`, so half the mobs decide on each tick.
    void tickRunning(MobContext& mob, bool all);

private:
    [[nodiscard]] bool canTakeFlags(const WrappedGoal& wrapped) const;

    std::vector<std::unique_ptr<WrappedGoal>> goals_;
    std::map<GoalFlag, WrappedGoal*> lockedFlags_;
};

} // namespace basalt::server::runtime::entity::ai
