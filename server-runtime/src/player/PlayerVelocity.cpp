#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerVelocity.hpp>

#include <cmath>

namespace basalt::server::runtime::player::velocity {
namespace {

constexpr double kDecay = 0.98;
constexpr double kHorizontalRest = 9.0E-6;
constexpr double kVerticalRest = 0.003;
/// parity: the player's own half width, which the slide's overlap is measured with.
constexpr double kHalfWidth = static_cast<double>(0.6F / 2.0F);

/// parity: `HoneyBlock.getOldDeltaY` and `getNewDeltaY`.
[[nodiscard]] double oldDeltaY(double deltaY) { return (deltaY / static_cast<double>(0.98F)) + 0.08; }

[[nodiscard]] double newDeltaY(double deltaY) { return (deltaY - 0.08) * static_cast<double>(0.98F); }

} // namespace

void decay(PlayerSession& player) {
    auto& [x, y, z] = player.deltaMovement;
    x *= kDecay;
    y *= kDecay;
    z *= kDecay;
    // parity: a player's horizontal motion is dropped as a whole, the others' axis by axis.
    if ((x * x) + (z * z) < kHorizontalRest) {
        x = 0.0;
        z = 0.0;
    }
    if (std::abs(y) < kVerticalRest) {
        y = 0.0;
    }
}

bool slidingDown(const PlayerSession& player, std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ) {
    if (player.onGround || player.y > static_cast<double>(blockY) + static_cast<double>(0.9375F) - 1.0E-7 ||
        oldDeltaY(player.deltaMovement[1]) >= -0.08) {
        return false;
    }
    const double dx = std::abs(static_cast<double>(blockX) + 0.5 - player.x);
    const double dz = std::abs(static_cast<double>(blockZ) + 0.5 - player.z);
    const double overlap = static_cast<double>(0.4375F) + kHalfWidth;
    return dx + 1.0E-7 > overlap || dz + 1.0E-7 > overlap;
}

void slide(PlayerSession& player) {
    auto& [x, y, z] = player.deltaMovement;
    const double old = oldDeltaY(y);
    if (old < -0.13) {
        const double reduction = -0.05 / old;
        x *= reduction;
        z *= reduction;
    }
    y = newDeltaY(-0.05);
}

} // namespace basalt::server::runtime::player::velocity
