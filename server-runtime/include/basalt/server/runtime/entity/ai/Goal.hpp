#pragma once

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

class MobContext;

/// parity: `Goal.Flag`.
enum class GoalFlag : std::uint8_t { Move, Look, Jump, Target };

/// parity: `Goal`.
class Goal {
public:
    Goal() = default;
    Goal(const Goal&) = delete;
    Goal& operator=(const Goal&) = delete;
    Goal(Goal&&) = delete;
    Goal& operator=(Goal&&) = delete;
    virtual ~Goal() = default;

    [[nodiscard]] virtual std::set<GoalFlag> flags() const { return {}; }

    [[nodiscard]] virtual bool canUse(MobContext& mob) = 0;

    [[nodiscard]] virtual bool canContinueToUse(MobContext& mob) { return canUse(mob); }

    /// parity: `Goal.isInterruptable`.
    [[nodiscard]] virtual bool isInterruptable() const { return true; }

    [[nodiscard]] virtual bool requiresUpdateEveryTick() const { return false; }

    virtual void start(MobContext& /*mob*/) {}

    virtual void stop(MobContext& /*mob*/) {}

    virtual void tick(MobContext& /*mob*/) {}

protected:
    /// parity: `Goal.adjustedTickDelay`, halved for the goals that are not ticked every tick.
    [[nodiscard]] std::int32_t adjustedTickDelay(std::int32_t ticks) const {
        return requiresUpdateEveryTick() ? ticks : reducedTickDelay(ticks);
    }

    [[nodiscard]] static std::int32_t reducedTickDelay(std::int32_t ticks) { return (ticks + 1) / 2; }
};

} // namespace basalt::server::runtime::entity::ai
