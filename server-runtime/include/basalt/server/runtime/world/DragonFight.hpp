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

/// The End's fight against its dragon, as the End remembers it.
///
/// parity: `EnderDragonFight`, saved as the `ender_dragon_fight` data with the same keys.
struct DragonFight {
    /// parity: `GATEWAY_COUNT`, the ring of return gateways opened one per kill.
    static constexpr std::int32_t kGatewayCount = 20;

    bool needsStateScanning = true;
    bool dragonKilled = false;
    bool previouslyKilled = false;
    std::optional<RespawnStage> respawnStage;
    std::int32_t respawnTime = 0;
    std::optional<wire::common::Uuid> dragonUuid;
    std::optional<std::array<std::int32_t, 3>> exitPortalLocation;
    /// Gateway indices still to open, the next one last.
    std::vector<std::int32_t> gateways;
    std::vector<wire::common::Uuid> respawnCrystals;

    // What the fight counts between saves, which vanilla does not keep either.
    std::int32_t ticksSinceDragonSeen = 0;
    std::int32_t aliveCrystals = 0;
    std::int32_t ticksSinceCrystalsScanned = 0;
    std::int32_t ticksSinceLastPlayerScan = 21;
    /// Whether this fight's `TicketType.DRAGON` is held in the world's forced chunks. Vanilla
    /// re-adds the ticket every tick while anyone fights and removes it when the arena empties;
    /// this remembers which of the forced chunks are ours to take back.
    bool dragonTicketHeld = false;
    /// The boss bar and who is being shown it.
    wire::common::Uuid barId = wire::common::Uuid::random();
    std::set<std::int32_t> barPlayers;
    bool barVisible = true;
    float barProgress = 1.0F;

    [[nodiscard]] wire::nbt::NbtCompound encode() const;

    [[nodiscard]] static DragonFight decode(const wire::nbt::NbtCompound& tag);

    /// parity: `EnderDragonFight.init`: the gateway order, shuffled from the seed the first time.
    void shuffleGateways(std::int64_t seed);
};

} // namespace basalt::server::runtime::world
