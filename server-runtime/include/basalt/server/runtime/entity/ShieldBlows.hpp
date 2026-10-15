#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace shieldBlows {

struct Push {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Reply {
    std::optional<Push> push;
    bool stunned = false;
};

[[nodiscard]] Reply blocked(ServerEntity& attacker, double defenderX, double defenderZ, double defenderResistance,
                            random::RandomSource& random);

[[nodiscard]] std::optional<Push> hoglinThrow(const ServerEntity& body, double targetX, double targetZ,
                                              double resistance, random::RandomSource& random);

[[nodiscard]] Push strongKnockback(const ServerEntity& ravager, double targetX, double targetZ);

[[nodiscard]] bool ravagerImmobile(const ServerEntity& ravager);

enum class RavagerTurn : std::uint8_t { Nothing, Roars, StunEnds };

[[nodiscard]] RavagerTurn tickRavager(ServerEntity& ravager, bool hasTarget);

} // namespace shieldBlows

} // namespace basalt::server::runtime::entity
