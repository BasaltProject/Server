#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a mob does when a shield takes its blow, and the ravager's own stun and roar that follow.
///
/// parity: `LivingEntity.blockedByItem` and its three overrides, `Ravager`, `Hoglin` and `Zoglin`.
namespace shieldBlows {

/// A shove on the defender, added to how it is moving.
struct Push {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Reply {
    std::optional<Push> push;
    /// The ravager was stunned rather than shoving: its stun event and sound are owed.
    bool stunned = false;
};

/// parity: `blockedByItem` for a shield that took all of it: nothing, but for the three that shove.
[[nodiscard]] Reply blocked(ServerEntity& attacker, double defenderX, double defenderZ, double defenderResistance,
                            random::RandomSource& random);

/// parity: `Ravager.strongKnockback`.
[[nodiscard]] Push strongKnockback(const ServerEntity& ravager, double targetX, double targetZ);

/// parity: `Ravager.isImmobile`, attacking, stunned or roaring.
[[nodiscard]] bool ravagerImmobile(const ServerEntity& ravager);

/// What one tick of a ravager's timers did.
enum class RavagerTurn : std::uint8_t { Nothing, Roars, StunEnds };

/// parity: `Ravager.aiStep`, the speed and the three timers. `hasTarget` picks the faster pace.
[[nodiscard]] RavagerTurn tickRavager(ServerEntity& ravager, bool hasTarget);

} // namespace shieldBlows

} // namespace basalt::server::runtime::entity
