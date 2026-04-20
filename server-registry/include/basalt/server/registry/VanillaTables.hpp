#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace basalt::server::registry {
struct BrewingMix {
    std::string_view kind;
    std::string_view from;
    std::string_view ingredients;
    std::string_view to;
};

struct FireOddsEntry {
    std::int32_t stateId;
    std::int32_t igniteOdds;
    std::int32_t burnOdds;
};

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> itemIdsTable();

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> entityTypeIdsTable();

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> particleTypeIdsTable();

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> dataComponentIdsTable();

[[nodiscard]] std::span<const std::string_view> blockEntityTypeNamesTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> blockEntityOwnersTable();

[[nodiscard]] std::span<const std::string_view> blockShapeSpecsTable();

[[nodiscard]] std::string_view blockShapeIndexTable();

[[nodiscard]] std::span<const std::string_view> supportRulesTable();

[[nodiscard]] std::span<const std::string_view> supportSetsTable();

[[nodiscard]] std::span<const std::int32_t> randomTickingStatesTable();

// parity: `FuelValues.vanillaBurnTimes`.
[[nodiscard]] std::span<const std::pair<std::string_view, std::int32_t>> fuelValuesTable();

// parity: `BlockStateBase.isRedstoneConductor`.
[[nodiscard]] std::span<const std::int32_t> redstoneConductorStatesTable();

// parity: `BlockState.getPistonPushReaction`: `BLOCK` stops it, `DESTROY` breaks.
[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> pushReactionsTable();

// parity: `PotionBrewing.bootstrap`, built in code and not in data.
[[nodiscard]] std::span<const BrewingMix> brewingMixesTable();

// parity: `WeatheringCopper.NEXT_BY_BLOCK`.
[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> weatheringStepsTable();

[[nodiscard]] std::span<const FireOddsEntry> fireOddsTable();

// parity: `BlockBehaviour.Properties.mapColor`, code rather than data.
[[nodiscard]] std::span<const std::pair<std::int32_t, std::int32_t>> blockMapColorsTable();

// parity: `MapColor.col`.
[[nodiscard]] std::span<const std::int32_t> mapColorsTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> blockFamiliesTable();

[[nodiscard]] std::span<const std::string_view> itemStackingTable();

[[nodiscard]] std::span<const std::string_view> itemArmourTable();

[[nodiscard]] std::span<const std::string_view> itemFoodTable();

[[nodiscard]] std::span<const std::string_view> itemWeaponsTable();

// parity: the state definitions of the two portal blocks, from the official `blocks.json`.
[[nodiscard]] std::span<const std::string_view> portalBlockStateSpecsTable();

// parity: every possible 26.3 state definition for every block referenced by a village.
[[nodiscard]] std::span<const std::string_view> villageBlockStateSpecsTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> blockInstrumentsTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> craftingRemaindersTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> itemBlocksTable();

[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> trimMaterialsTable();

[[nodiscard]] std::string_view blockSupportIndexTable();
} // namespace basalt::server::registry
