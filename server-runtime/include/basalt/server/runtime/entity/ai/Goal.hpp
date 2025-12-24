#pragma once

#include <cstdint>
#include <set>

namespace basalt::server::runtime::entity::ai {

class MobContext;

/// What a goal takes hold of while it runs.
///
/// parity: `Goal.Flag`. Two goals that want the same flag cannot run together, and the one with the
/// lower priority number wins: that is the whole of how vanilla decides what a mob is doing.
enum class GoalFlag : std::uint8_t { Move, Look, Jump, Target };

/// One thing a mob might be doing. parity: `Goal`, asked whether it can start, kept while it says it
/// can continue, and ticked in between; the instance belongs to one mob, so it remembers its own
/// counters.
///
/// Virtual, unlike the block and packet dispatch elsewhere: a mob runs a handful per tick, and a
/// table here would be the same function pointers with none of the readability.
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

    /// parity: `Goal.isInterruptable`, which is what stops a panicking mob being talked out of it.
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
