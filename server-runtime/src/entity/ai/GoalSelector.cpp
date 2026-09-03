#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::entity::ai {

WrappedGoal::WrappedGoal(std::int32_t priority, std::unique_ptr<Goal> goal)
    : priority_(priority), goal_(std::move(goal)) {}

bool WrappedGoal::canBeReplacedBy(const WrappedGoal& other) const {
    return goal_->isInterruptable() && other.priority_ < priority_;
}

void WrappedGoal::start(MobContext& mob) {
    if (running_) {
        return;
    }
    running_ = true;
    goal_->start(mob);
}

void WrappedGoal::stop(MobContext& mob) {
    if (!running_) {
        return;
    }
    running_ = false;
    goal_->stop(mob);
}

void GoalSelector::add(std::int32_t priority, std::unique_ptr<Goal> goal) {
    goals_.push_back(std::make_unique<WrappedGoal>(priority, std::move(goal)));
}

bool GoalSelector::isRunning(const std::function<bool(const Goal&)>& predicate) const {
    return std::ranges::any_of(goals_, [&predicate](const std::unique_ptr<WrappedGoal>& wrapped) {
        return wrapped->running() && predicate(wrapped->goal());
    });
}

bool GoalSelector::canTakeFlags(const WrappedGoal& wrapped) const {
    for (const GoalFlag flag : wrapped.goal().flags()) {
        const auto held = lockedFlags_.find(flag);
        if (held != lockedFlags_.end() && !held->second->canBeReplacedBy(wrapped)) {
            return false;
        }
    }
    return true;
}

void GoalSelector::tick(MobContext& mob) {
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running() && !wrapped->goal().canContinueToUse(mob)) {
            wrapped->stop(mob);
        }
    }
    std::erase_if(lockedFlags_, [](const auto& entry) { return !entry.second->running(); });
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running() || !canTakeFlags(*wrapped) || !wrapped->goal().canUse(mob)) {
            continue;
        }
        for (const GoalFlag flag : wrapped->goal().flags()) {
            if (const auto held = lockedFlags_.find(flag); held != lockedFlags_.end()) {
                held->second->stop(mob);
            }
            lockedFlags_.insert_or_assign(flag, wrapped.get());
        }
        wrapped->start(mob);
    }
    tickRunning(mob, true);
}

void GoalSelector::tickRunning(MobContext& mob, bool all) {
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running() && (all || wrapped->goal().requiresUpdateEveryTick())) {
            wrapped->goal().tick(mob);
        }
    }
}

} // namespace basalt::server::runtime::entity::ai
