#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobBreathing {

struct Surroundings {
    bool eyeInWater = false;
    bool eyeInBubbles = false;
    bool inWater = false;
    bool inRain = false;
    /// parity: `Attributes.OXYGEN_BONUS`.
    double oxygenBonus = 0.0;
    random::RandomSource* random = nullptr;
};

struct Hurt {
    float amount = 0.0F;
    std::string_view cause;
    /// parity: `EntityEvent.DROWN_PARTICLES`, which goes out just before a drowning hit.
    bool drownParticles = false;
};

[[nodiscard]] std::int32_t maxAirOf(std::string_view typePath);

[[nodiscard]] bool breathesUnderwater(std::string_view typePath);

[[nodiscard]] double eyeHeightOf(const ServerEntity& mob);

[[nodiscard]] std::vector<Hurt> tick(ServerEntity& mob, const Surroundings& at);

} // namespace mobBreathing

} // namespace basalt::server::runtime::entity
