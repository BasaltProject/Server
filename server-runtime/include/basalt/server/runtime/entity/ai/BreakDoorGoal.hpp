#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime::entity::ai {

/// parity: `BreakDoorGoal`, a zombie that may break doors batters a wooden one on its path on hard difficulty.
class BreakDoorGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void start(MobContext& mob) override;

    void stop(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    [[nodiscard]] bool isOpen(MobContext& mob);

    std::array<std::int32_t, 3> doorPos_{};
    bool hasDoor_ = false;
    std::int32_t breakTime_ = 0;
    std::int32_t lastBreakProgress_ = -1;
};

} // namespace basalt::server::runtime::entity::ai
