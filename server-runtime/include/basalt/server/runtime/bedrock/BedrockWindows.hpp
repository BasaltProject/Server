#pragma once

#include <basalt/server/runtime/world/Enchanting.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Trades.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::bedrock {

struct OpenWindow {
    std::int32_t containerId = 0;
    wire::bedrock::BedrockContainerType type = wire::bedrock::BedrockContainerType::Container;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::optional<std::array<std::int32_t, 3>> second;
    std::int32_t size = 0;
    std::string block;
    bool playerOwned = false;
};

struct OpenTrades {
    std::int32_t entityId = 0;
    std::int32_t tier = 0;
    std::vector<world::trades::Offer> offers;
};

struct EnchantRow {
    std::int32_t cost = 0;
    std::int32_t netId = 0;
    std::vector<world::enchanting::Offer> offers;
};

struct OpenEnchantment {
    std::array<EnchantRow, 3> rows;
};

/// parity: `CraftingMenu`'s own three by three.
inline constexpr std::size_t kGridCells = 9;

struct FurnaceNumbers {
    std::int32_t cookTime = 0;
    std::int32_t litTime = 0;
    std::int32_t litDuration = 0;

    friend bool operator==(const FurnaceNumbers&, const FurnaceNumbers&) = default;
};

struct BedrockWindows {
    std::optional<storage::StoredItemStack> cursor;
    std::array<std::optional<storage::StoredItemStack>, kGridCells> grid;
    std::optional<OpenWindow> open;
    std::optional<OpenTrades> trades;
    std::optional<OpenEnchantment> enchanting;
    std::int32_t nextEnchantNetId = 0;
    std::vector<std::optional<storage::StoredItemStack>> saidSlots;
    std::optional<FurnaceNumbers> saidNumbers;
    std::int32_t sinceRead = 0;
    std::int32_t nextContainerId = 0;
};

/// parity: `ClientSession`.
inline constexpr std::int32_t kMaxContainerId = 100;

[[nodiscard]] std::optional<OpenWindow> windowFor(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

[[nodiscard]] std::optional<std::int32_t> blockSlotOf(const OpenWindow& window, std::int32_t container,
                                                      std::int32_t slot);

[[nodiscard]] std::optional<FurnaceNumbers> furnaceNumbers(world::LoadedWorld& world, const OpenWindow& window);

[[nodiscard]] std::vector<std::optional<storage::StoredItemStack>>
readWindow(world::LoadedWorld& world, const OpenWindow& window, const player::PlayerSession* owner);

[[nodiscard]] std::optional<storage::StoredItemStack> readWindowSlot(world::LoadedWorld& world,
                                                                     const OpenWindow& window, std::int32_t slot,
                                                                     const player::PlayerSession* owner);

bool writeWindowSlot(world::LoadedWorld& world, const OpenWindow& window, std::int32_t slot,
                     const std::optional<storage::StoredItemStack>& stack, player::PlayerSession* owner);

} // namespace basalt::server::runtime::bedrock
