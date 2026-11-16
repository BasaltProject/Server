#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::entity::ai {

WrappedGoal::WrappedGoal(std::int32_t priority, std::unique_ptr<Goal> goal, std::string name, bool plugin)
    : priority_(priority), goal_(std::move(goal)), name_(std::move(name)), flags_(goal_->flags()), plugin_(plugin) {
    for (const GoalFlag flag : flags_) {
        flagMask_ = static_cast<std::uint8_t>(flagMask_ | (1U << static_cast<unsigned>(flag)));
    }
}

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

void GoalSelector::add(std::int32_t priority, std::unique_ptr<Goal> goal, std::string name, bool plugin) {
    (ticking_ ? pending_ : goals_)
        .push_back(std::make_unique<WrappedGoal>(priority, std::move(goal), std::move(name), plugin));
    pluginGoals_ += plugin ? 1 : 0;
}

std::vector<GoalSelector::Row> GoalSelector::rows() const {
    std::vector<Row> out;
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (!wrapped->retired()) {
            out.push_back(Row{.name = wrapped->name(),
                              .priority = wrapped->priority(),
                              .running = wrapped->running(),
                              .plugin = wrapped->plugin(),
                              .usable = wrapped->lastUsable(),
                              .flags = wrapped->goal().flags()});
        }
    }
    return out;
}

std::size_t GoalSelector::remove(std::string_view name) {
    std::size_t count = 0;
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (!wrapped->retired() && wrapped->name() == name) {
            wrapped->retire();
            dirty_ = true;
            ++count;
        }
    }
    return count;
}

std::size_t GoalSelector::removeAll(const std::set<GoalFlag>& flags) {
    std::size_t count = 0;
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->retired()) {
            continue;
        }
        const std::set<GoalFlag> own = wrapped->goal().flags();
        const bool match =
            flags.empty() || std::ranges::any_of(own, [&flags](GoalFlag flag) { return flags.contains(flag); });
        if (match) {
            wrapped->retire();
            dirty_ = true;
            ++count;
        }
    }
    return count;
}

bool GoalSelector::has(std::string_view name) const {
    return std::ranges::any_of(goals_, [name](const std::unique_ptr<WrappedGoal>& wrapped) {
        return !wrapped->retired() && wrapped->name() == name;
    });
}

void GoalSelector::sweep(MobContext& mob) {
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->retired()) {
            wrapped->stop(mob);
        }
    }
    for (WrappedGoal*& held : lockedFlags_) {
        if (held != nullptr && held->retired()) {
            held = nullptr;
        }
    }
    pluginGoals_ -= static_cast<std::size_t>(std::ranges::count_if(
        goals_, [](const std::unique_ptr<WrappedGoal>& wrapped) { return wrapped->retired() && wrapped->plugin(); }));
    std::erase_if(goals_, [](const std::unique_ptr<WrappedGoal>& wrapped) { return wrapped->retired(); });
}

bool GoalSelector::isRunning(const std::function<bool(const Goal&)>& predicate) const {
    return std::ranges::any_of(goals_, [&predicate](const std::unique_ptr<WrappedGoal>& wrapped) {
        return wrapped->running() && predicate(wrapped->goal());
    });
}

bool GoalSelector::canTakeFlags(const WrappedGoal& wrapped) const {
    for (std::size_t flag = 0; flag < lockedFlags_.size(); ++flag) {
        const WrappedGoal* held = lockedFlags_[flag];
        if (((wrapped.flagMask() >> flag) & 1U) != 0 && held != nullptr && !held->canBeReplacedBy(wrapped)) {
            return false;
        }
    }
    return true;
}

void GoalSelector::flushPending() {
    for (std::unique_ptr<WrappedGoal>& added : pending_) {
        goals_.push_back(std::move(added));
    }
    pending_.clear();
}

void GoalSelector::tick(MobContext& mob) {
    ticking_ = true;
    if (dirty_) {
        dirty_ = false;
        sweep(mob);
    }
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running()) {
            const bool keepGoing = wrapped->goal().canContinueToUse(mob);
            wrapped->noteUsable(keepGoing);
            if (!keepGoing) {
                wrapped->stop(mob);
            }
        }
    }
    for (WrappedGoal*& held : lockedFlags_) {
        if (held != nullptr && !held->running()) {
            held = nullptr;
        }
    }
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running() || wrapped->retired()) {
            continue;
        }
        if (!canTakeFlags(*wrapped)) {
            wrapped->noteUsable(false);
            continue;
        }
        const bool usable = wrapped->goal().canUse(mob);
        wrapped->noteUsable(usable);
        if (!usable) {
            continue;
        }
        for (std::size_t flag = 0; flag < lockedFlags_.size(); ++flag) {
            if (((wrapped->flagMask() >> flag) & 1U) == 0) {
                continue;
            }
            WrappedGoal*& held = lockedFlags_[flag];
            if (held != nullptr) {
                held->stop(mob);
            }
            held = wrapped.get();
        }
        wrapped->start(mob);
    }
    tickRunning(mob, true);
}

void GoalSelector::tickRunning(MobContext& mob, bool all) {
    ticking_ = true;
    for (const std::unique_ptr<WrappedGoal>& wrapped : goals_) {
        if (wrapped->running() && !wrapped->retired() && (all || wrapped->goal().requiresUpdateEveryTick())) {
            wrapped->goal().tick(mob);
        }
    }
    ticking_ = false;
    flushPending();
}

} // namespace basalt::server::runtime::entity::ai
