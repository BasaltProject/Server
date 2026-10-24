#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `WrappedGoal`.
class WrappedGoal {
public:
    WrappedGoal(std::int32_t priority, std::unique_ptr<Goal> goal, std::string name, bool plugin);

    [[nodiscard]] std::int32_t priority() const noexcept { return priority_; }

    [[nodiscard]] Goal& goal() const noexcept { return *goal_; }

    [[nodiscard]] bool running() const noexcept { return running_; }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    [[nodiscard]] bool plugin() const noexcept { return plugin_; }

    [[nodiscard]] bool retired() const noexcept { return retired_; }

    void retire() noexcept { retired_ = true; }

    [[nodiscard]] bool canBeReplacedBy(const WrappedGoal& other) const;

    void start(MobContext& mob);

    void stop(MobContext& mob);

private:
    std::int32_t priority_;
    std::unique_ptr<Goal> goal_;
    std::string name_;
    bool plugin_ = false;
    bool retired_ = false;
    bool running_ = false;
};

/// parity: `GoalSelector.tick`.
class GoalSelector {
public:
    void add(std::int32_t priority, std::unique_ptr<Goal> goal, std::string name = {}, bool plugin = false);

    struct Row {
        std::string name;
        std::int32_t priority = 0;
        bool running = false;
        bool plugin = false;
        std::set<GoalFlag> flags;
    };

    [[nodiscard]] std::vector<Row> rows() const;

    /// Retires every live goal of that name; a running one is stopped on the next tick.
    std::size_t remove(std::string_view name);

    /// Retires every goal that locks the flag, or all of them without one.
    std::size_t removeAll(const std::set<GoalFlag>& flags);

    [[nodiscard]] bool has(std::string_view name) const;

    [[nodiscard]] bool hasPluginGoals() const noexcept { return pluginGoals_ > 0; }

    [[nodiscard]] std::size_t size() const noexcept { return goals_.size(); }

    [[nodiscard]] bool isRunning(const std::function<bool(const Goal&)>& predicate) const;

    void tick(MobContext& mob);

    /// parity: `GoalSelector.tickRunningGoals`.
    void tickRunning(MobContext& mob, bool all);

private:
    [[nodiscard]] bool canTakeFlags(const WrappedGoal& wrapped) const;

    void sweep(MobContext& mob);

    void flushPending();

    std::vector<std::unique_ptr<WrappedGoal>> goals_;
    std::vector<std::unique_ptr<WrappedGoal>> pending_;
    std::size_t pluginGoals_ = 0;
    bool ticking_ = false;
    bool dirty_ = false;
    std::map<GoalFlag, WrappedGoal*> lockedFlags_;
};

} // namespace basalt::server::runtime::entity::ai
