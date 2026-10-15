#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string_view>

namespace basalt::server::runtime::entity::ai {

[[nodiscard]] double distanceBetween(double ax, double ay, double az, double bx, double by, double bz);

/// parity: `Spider.onClimbable`.
class ClimbWallsGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `EnderMan.teleport`.
class TeleportAwayGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;

private:
    static void teleportRandomly(MobContext& mob);
};

/// parity: `Enderman.teleport(x, y, z)`.
bool teleportEnderman(MobContext& mob, double x, double y, double z);

/// parity: `Enderman.teleport()`, anywhere within thirty-two across and up or down.
bool teleportEndermanRandomly(MobContext& mob);

/// parity: 26.3 `LivingEntity.randomTeleport`.
[[nodiscard]] std::optional<double> landingFor(MobContext& mob, double x, double y, double z, std::string_view avoided);

using BlockTagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

/// parity: 26.3 `LivingEntity.randomTeleport` and `checkPositionAndTeleport`.
[[nodiscard]] std::optional<double> randomTeleportLanding(const world::WorldState& world, const BlockTagged& tagged,
                                                          double x, double y, double z, double width, double height,
                                                          std::string_view avoided, bool waterBelowBlocks);

} // namespace basalt::server::runtime::entity::ai
