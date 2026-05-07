#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobSteps {

enum class Emission : std::uint8_t { None, Events, All };

[[nodiscard]] Emission emissionOf(const wire::common::Identifier& type);

struct Sound {
    std::string name;
    float volume = 1.0F;
    float pitch = 1.0F;
};

using Tagged = std::function<bool(std::string_view tag, std::int32_t stateId)>;

[[nodiscard]] std::optional<Sound> ownStep(ServerEntity& mob, std::int32_t stateId, std::int32_t above, bool inLava,
                                           const Tagged& tagged);

[[nodiscard]] Sound blockStep(std::int32_t stateId);

[[nodiscard]] std::string swimSound(const ServerEntity& mob);

[[nodiscard]] float swimVolume(const ServerEntity& mob, float volume);

/// parity: an empty `checkFallDamage`, which the fliers have: they never count a fall.
[[nodiscard]] bool countsFalls(const wire::common::Identifier& type);

[[nodiscard]] std::string fallSound(const wire::common::Identifier& type, std::int32_t damage);

[[nodiscard]] std::int32_t fallDamageReduction(const wire::common::Identifier& type);

} // namespace mobSteps

} // namespace basalt::server::runtime::entity
