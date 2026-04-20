#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace witherBoss {

inline constexpr std::uint8_t kFirstTargetDataIndex = 16;
inline constexpr std::uint8_t kInvulnerableDataIndex = 19;

inline constexpr std::int32_t kInvulnerableTicks = 220;
inline constexpr double kMaxHealth = 300.0;
inline constexpr std::string_view kInvulnerableKey = "Invul";

struct WitherState {
    std::int32_t invulnerableTicks = 0;
    std::array<std::int32_t, 3> targets{};
    std::array<std::int32_t, 2> nextHeadUpdate{};
    std::array<std::int32_t, 2> idleHeadUpdates{};
    std::int32_t destroyBlocksTick = 0;
    /// Set just before a hit whose projectile or blast an undead owns; the hurt clears it.
    bool nextHitFromFriend = false;
    random::RandomSource random = random::RandomSource::legacy(0);
};

[[nodiscard]] bool isWither(const wire::common::Identifier& type);

[[nodiscard]] bool isFriend(std::string_view typePath);

[[nodiscard]] bool isPowered(const ServerEntity& wither);

[[nodiscard]] std::array<double, 3> headPosition(const ServerEntity& wither, std::int32_t head);

void makeInvulnerable(ServerEntity& wither, WitherState& state);

struct Hit {
    bool fromWitherFriend = false;
    bool arrowOrWindCharge = false;
    bool bypassesInvulnerability = false;
    bool witherImmuneType = false;
};

[[nodiscard]] bool refuses(const ServerEntity& wither, WitherState& state, const Hit& hit);

} // namespace witherBoss

} // namespace basalt::server::runtime::entity
