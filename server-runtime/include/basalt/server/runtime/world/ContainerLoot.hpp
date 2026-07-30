#pragma once

#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

/// parity: `RandomizableContainer`.
namespace containerLoot {

inline constexpr std::string_view kTableKey = "LootTable";
inline constexpr std::string_view kSeedKey = "LootTableSeed";

/// The player who opened it, as the roll sees them. parity: `THIS_ENTITY` and `withLuck`.
struct Opener {
    std::string name;
    float luck = 0.0F;
    std::optional<wire::common::Identifier> mainhand;
};

struct Lootable {
    std::int64_t lastFill = -1;
    std::int64_t nextRefill = -1;
    std::int32_t numRefills = 0;
    std::vector<std::pair<wire::common::Uuid, std::int64_t>> looted;

    [[nodiscard]] static Lootable read(const wire::nbt::NbtCompound& data);

    void write(wire::nbt::NbtCompound& data) const;

    [[nodiscard]] bool canLoot(const wire::common::Uuid& player, std::int64_t now, std::int32_t relootSeconds) const;

    /// parity: Paper's `PaperLootableInventoryData.shouldReplenish`, with the event left out.
    [[nodiscard]] bool shouldRefill(const std::optional<wire::common::Uuid>& player, std::int64_t now, bool refill,
                                    std::int32_t relootSeconds, std::int32_t maxRefills) const;
};

/// A zero seed rolls on the table's own stream, as the jar does.
void fill(LoadedWorld& world, RealLootTables& tables, std::string_view table, std::int64_t seed,
          std::vector<std::optional<server::world::loot::ItemStack>>& slots, bool byPlayer,
          const std::array<std::int32_t, 3>& at, const std::optional<Opener>& opener = std::nullopt);

/// parity: `RandomizableContainer.unpackLootTable`.
std::optional<wire::common::Identifier> unpack(LoadedWorld& world, RealLootTables& tables, std::int32_t x,
                                               std::int32_t y, std::int32_t z,
                                               const std::optional<wire::common::Uuid>& player,
                                               const std::optional<Opener>& opener = std::nullopt);

} // namespace containerLoot

} // namespace basalt::server::runtime::world
