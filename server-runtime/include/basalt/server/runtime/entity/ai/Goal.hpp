#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

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

/// The goal's registry name: `PanicGoal` is `panic`, the key a plugin removes it by.
template <typename GoalType>
[[nodiscard]] std::string goalNameOf() {
    constexpr std::string_view kSignature = __PRETTY_FUNCTION__;
    const std::size_t equals = kSignature.rfind("= ");
    std::string_view full = kSignature.substr(equals + 2);
    full = full.substr(0, full.find_first_of("];"));
    if (const std::size_t scope = full.rfind("::"); scope != std::string_view::npos) {
        full.remove_prefix(scope + 2);
    }
    if (full.ends_with("Goal")) {
        full.remove_suffix(4);
    }
    std::string name;
    for (const char letter : full) {
        if (letter >= 'A' && letter <= 'Z') {
            if (!name.empty()) {
                name.push_back('_');
            }
            name.push_back(static_cast<char>(letter - 'A' + 'a'));
        } else {
            name.push_back(letter);
        }
    }
    return name;
}

} // namespace basalt::server::runtime::entity::ai
