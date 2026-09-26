#pragma once

#include <basalt/server/runtime/entity/ai/Goals.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string_view>

namespace basalt::server::runtime::entity::ai {

/// How far apart two points are, for the goals that measure it themselves.
[[nodiscard]] double distanceBetween(double ax, double ay, double az, double bx, double by, double bz);

/// parity: `Spider.onClimbable`, which is simply "am I walking into something": while it is, the
/// climb replaces the fall, which is what takes a spider straight up the side of a house.
class ClimbWallsGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override;

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override;

    void tick(MobContext& mob) override;
};

/// parity: `EnderMan.teleport`, `teleportTowards` and `customServerAiStep`: it goes away when it is
/// hit, when it is rained on or standing in water, and when the sun catches it holding no target.
class TeleportAwayGoal final : public Goal {
public:
    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& /*mob*/) override { return true; }

    void tick(MobContext& mob) override;

private:
    static void teleportRandomly(MobContext& mob);
};

/// parity: `Enderman.teleport(x, y, z)`. Answers whether it moved.
bool teleportEnderman(MobContext& mob, double x, double y, double z);

/// parity: `Enderman.teleport()`, anywhere within thirty-two across and up or down.
bool teleportEndermanRandomly(MobContext& mob);

/// Where a mob lands teleporting to around `x, y, z`, or nothing when it may not.
///
/// parity: 26.3 `LivingEntity.randomTeleport` with the block tag the mob avoids, and the enderman's
/// `canRandomlyTeleportTo`: no water under its feet.
[[nodiscard]] std::optional<double> landingFor(MobContext& mob, double x, double y, double z, std::string_view avoided);

/// Whether a block state is in a block tag.
using BlockTagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

/// Where a box of this size lands teleporting to around `x, y, z`, or nothing when it may not.
/// parity: 26.3 `LivingEntity.randomTeleport` and `checkPositionAndTeleport`; `waterBelowBlocks` is
/// the enderman's own `canRandomlyTeleportTo`.
[[nodiscard]] std::optional<double> randomTeleportLanding(const world::WorldState& world, const BlockTagged& tagged,
                                                          double x, double y, double z, double width, double height,
                                                          std::string_view avoided, bool waterBelowBlocks);

} // namespace basalt::server::runtime::entity::ai
