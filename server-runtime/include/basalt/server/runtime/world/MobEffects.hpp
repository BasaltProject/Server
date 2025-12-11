#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// One status effect on a player: what it is, how strong, and how long it has left.
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

/// The `minecraft:mob_effect` registry, which is what the effect packets carry.
///
/// Ids rather than names: the packets send the registry id, and the client resolves it against its
/// own built-in registry, so this table is protocol fact and not content.
namespace mobEffects {

[[nodiscard]] std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& effect);

[[nodiscard]] std::optional<wire::common::Identifier> byProtocolId(std::int32_t id);

[[nodiscard]] std::vector<wire::common::Identifier> ids();

/// parity: the four that act on a tick rather than by being present.
[[nodiscard]] const wire::common::Identifier& regeneration();
[[nodiscard]] const wire::common::Identifier& poison();
[[nodiscard]] const wire::common::Identifier& wither();
[[nodiscard]] const wire::common::Identifier& hunger();

/// parity: the two that apply once, the moment they are given.
[[nodiscard]] const wire::common::Identifier& instantHealth();
[[nodiscard]] const wire::common::Identifier& instantDamage();

[[nodiscard]] const wire::common::Identifier& haste();
[[nodiscard]] const wire::common::Identifier& miningFatigue();

/// parity: the three a raid runs on: the omen, what it turns into, and what winning gives.
[[nodiscard]] const wire::common::Identifier& badOmen();
[[nodiscard]] const wire::common::Identifier& raidOmen();
[[nodiscard]] const wire::common::Identifier& heroOfTheVillage();

/// parity: the two that change what a hit is worth rather than what it hits.
[[nodiscard]] const wire::common::Identifier& resistance();
[[nodiscard]] const wire::common::Identifier& absorption();

} // namespace mobEffects

} // namespace basalt::server::runtime::world
