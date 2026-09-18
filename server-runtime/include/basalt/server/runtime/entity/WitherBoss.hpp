#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The wither: its three heads, its long birth and what it will not be hurt by.
///
/// parity: `WitherBoss`. What reaches into the world is `WitherSystem`'s.
namespace witherBoss {

/// parity: `DATA_TARGET_A` to `C` and `DATA_ID_INV`.
inline constexpr std::uint8_t kFirstTargetDataIndex = 16;
inline constexpr std::uint8_t kInvulnerableDataIndex = 19;

/// parity: `INVULNERABLE_TICKS`, the eleven seconds it spends growing.
inline constexpr std::int32_t kInvulnerableTicks = 220;
inline constexpr double kMaxHealth = 300.0;
inline constexpr std::string_view kInvulnerableKey = "Invul";

struct WitherState {
    std::int32_t invulnerableTicks = 0;
    /// The entity each head looks at, the middle one first, zero for none.
    std::array<std::int32_t, 3> targets{};
    std::array<std::int32_t, 2> nextHeadUpdate{};
    std::array<std::int32_t, 2> idleHeadUpdates{};
    std::int32_t destroyBlocksTick = 0;
    /// Set just before a hit whose projectile or blast an undead owns; the hurt clears it.
    bool nextHitFromFriend = false;
    /// parity: `Entity.random`.
    random::RandomSource random = random::RandomSource::legacy(0);
};

[[nodiscard]] bool isWither(const wire::common::Identifier& type);

/// parity: `#minecraft:wither_friends`, which is `#minecraft:undead`.
[[nodiscard]] bool isFriend(std::string_view typePath);

/// parity: `WitherBoss.isPowered`, the armour it grows at half health.
[[nodiscard]] bool isPowered(const ServerEntity& wither);

/// parity: `getHeadX`, `getHeadY` and `getHeadZ`: the middle head on top, the others to either side.
[[nodiscard]] std::array<double, 3> headPosition(const ServerEntity& wither, std::int32_t head);

/// parity: `WitherBoss.makeInvulnerable`, a third of its health and eleven seconds untouchable.
void makeInvulnerable(ServerEntity& wither, WitherState& state);

/// Why a hit does not land on it, before the ordinary rules are asked.
struct Hit {
    bool fromWitherFriend = false;
    bool arrowOrWindCharge = false;
    bool bypassesInvulnerability = false;
    bool witherImmuneType = false;
};

/// parity: the refusals at the top of `WitherBoss.hurtServer`. On a hit that lands it also books
/// the block breaking and hurries the side heads.
[[nodiscard]] bool refuses(const ServerEntity& wither, WitherState& state, const Hit& hit);

} // namespace witherBoss

} // namespace basalt::server::runtime::entity
