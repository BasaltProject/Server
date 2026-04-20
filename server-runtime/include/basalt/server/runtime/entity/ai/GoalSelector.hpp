#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `WrappedGoal`.
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

/// parity: `GoalSelector.tick`.
class GoalSelector {
public:
    void add(std::int32_t priority, std::unique_ptr<Goal> goal);

    [[nodiscard]] std::size_t size() const noexcept { return goals_.size(); }

    [[nodiscard]] bool isRunning(const std::function<bool(const Goal&)>& predicate) const;

    void tick(MobContext& mob);

    /// parity: `GoalSelector.tickRunningGoals`.
    void tickRunning(MobContext& mob, bool all);

private:
    [[nodiscard]] bool canTakeFlags(const WrappedGoal& wrapped) const;

    std::vector<std::unique_ptr<WrappedGoal>> goals_;
    std::map<GoalFlag, WrappedGoal*> lockedFlags_;
};

} // namespace basalt::server::runtime::entity::ai
