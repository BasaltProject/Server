#pragma once

#include <basalt/server/runtime/entity/ai/Goal.hpp>

namespace basalt::server::runtime::entity::ai {

/// parity: `LandOnOwnersShoulderGoal`.
class LandOnOwnersShoulderGoal final : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool isInterruptable() const override { return !sittingOnShoulder_; }

    void start(MobContext& mob) override;

    void tick(MobContext& mob) override;

private:
    bool sittingOnShoulder_ = false;
};

} // namespace basalt::server::runtime::entity::ai
