#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

struct ActiveEffect {
    wire::common::Identifier effect;
    std::int32_t amplifier = 0;
    std::int32_t remainingTicks = 0;
    bool ambient = false;
    bool visible = true;
    bool showIcon = true;

    /// parity: an infinite effect is -1 on the wire and never counts down.
    [[nodiscard]] bool infinite() const noexcept { return remainingTicks < 0; }

    friend bool operator==(const ActiveEffect&, const ActiveEffect&) = default;
};

/// Ids rather than names: packets send the registry id, so this table is protocol fact and not content.
namespace mobEffects {

[[nodiscard]] std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& effect);

[[nodiscard]] std::optional<wire::common::Identifier> byProtocolId(std::int32_t id);

[[nodiscard]] std::vector<wire::common::Identifier> ids();

/// parity: the colour an effect is registered with, which its `entity_effect` swirl is drawn in.
[[nodiscard]] std::optional<std::int32_t> colorOf(const wire::common::Identifier& effect);

[[nodiscard]] std::optional<std::string_view> ownParticleOf(const wire::common::Identifier& effect);

[[nodiscard]] const wire::common::Identifier& regeneration();
[[nodiscard]] const wire::common::Identifier& poison();
[[nodiscard]] const wire::common::Identifier& wither();
[[nodiscard]] const wire::common::Identifier& hunger();

[[nodiscard]] const wire::common::Identifier& instantHealth();
[[nodiscard]] const wire::common::Identifier& instantDamage();

[[nodiscard]] const wire::common::Identifier& haste();
[[nodiscard]] const wire::common::Identifier& darkness();
[[nodiscard]] const wire::common::Identifier& miningFatigue();

[[nodiscard]] const wire::common::Identifier& badOmen();
[[nodiscard]] const wire::common::Identifier& raidOmen();
[[nodiscard]] const wire::common::Identifier& heroOfTheVillage();

[[nodiscard]] const wire::common::Identifier& resistance();
[[nodiscard]] const wire::common::Identifier& absorption();

} // namespace mobEffects

} // namespace basalt::server::runtime::world
