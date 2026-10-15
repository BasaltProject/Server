#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `DragonRespawnStage`, by the name it is saved under.
enum class RespawnStage : std::uint8_t { Start, PreparingToSummonPillars, SummoningPillars, SummoningDragon, End };

[[nodiscard]] std::string_view respawnStageName(RespawnStage stage) noexcept;

[[nodiscard]] std::optional<RespawnStage> respawnStageByName(std::string_view name) noexcept;

/// parity: `EnderDragonFight`, saved as `ender_dragon_fight` with the same keys.
struct DragonFight {
    static constexpr std::int32_t kGatewayCount = 20;

    bool needsStateScanning = true;
    bool dragonKilled = false;
    bool previouslyKilled = false;
    std::optional<RespawnStage> respawnStage;
    std::int32_t respawnTime = 0;
    std::optional<wire::common::Uuid> dragonUuid;
    std::optional<std::array<std::int32_t, 3>> exitPortalLocation;
    std::vector<std::int32_t> gateways;
    std::vector<wire::common::Uuid> respawnCrystals;

    std::int32_t ticksSinceDragonSeen = 0;
    std::int32_t aliveCrystals = 0;
    std::int32_t ticksSinceCrystalsScanned = 0;
    std::int32_t ticksSinceLastPlayerScan = 21;
    /// Vanilla re-adds `TicketType.DRAGON` every tick while anyone fights; this remembers which forced chunks are ours.
    bool dragonTicketHeld = false;
    wire::common::Uuid barId = wire::common::Uuid::random();
    std::set<std::int32_t> barPlayers;
    bool barVisible = true;
    float barProgress = 1.0F;

    [[nodiscard]] wire::nbt::NbtCompound encode() const;

    [[nodiscard]] static DragonFight decode(const wire::nbt::NbtCompound& tag);

    /// parity: `EnderDragonFight.init`.
    void shuffleGateways(std::int64_t seed);
};

} // namespace basalt::server::runtime::world
