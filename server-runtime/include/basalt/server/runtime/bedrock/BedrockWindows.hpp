#pragma once

#include <basalt/server/runtime/world/LoadedWorld.hpp>
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

/// A block's window, as one Bedrock client has it open.
struct OpenWindow {
    /// This server's own number for it, which comes back on everything the client says about it.
    std::int32_t containerId = 0;
    wire::bedrock::BedrockContainerType type = wire::bedrock::BedrockContainerType::Container;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// The other half of a double chest. One window over two block entities, first half first.
    std::optional<std::array<std::int32_t, 3>> second;
    /// How many of the block's own slots the window shows, both halves of a chest counted. Zero
    /// for a window whose slots are not the block's: a table's grid belongs to whoever is at it.
    std::int32_t size = 0;
    /// The block's own path, because what a container keeps its stacks under is per block.
    std::string block;
    /// True for the one window whose slots are not in the world at all: an ender chest shows the
    /// player's own twenty-seven, which travel with them from world to world.
    bool playerOwned = false;
};

/// The nine cells of a crafting table. parity: `CraftingMenu`'s own three by three.
inline constexpr std::size_t kGridCells = 9;

/// The three numbers a furnace window draws its fire and its arrow from.
struct FurnaceNumbers {
    std::int32_t cookTime = 0;
    std::int32_t litTime = 0;
    std::int32_t litDuration = 0;

    friend bool operator==(const FurnaceNumbers&, const FurnaceNumbers&) = default;
};

/// What one Bedrock client is holding outside its slots, and which block it has open.
///
/// Shared rather than owned by the player, because the world thread is what moves both and a call
/// already on its way there outlives the player it was made for.
struct BedrockWindows {
    /// What is being dragged. That edition keeps it in a window of its own rather than outside the
    /// windows, but it is still one stack that belongs to nobody until it is put down.
    std::optional<storage::StoredItemStack> cursor;
    /// The nine cells of an open table's grid. They are nobody's while they sit there, so closing
    /// the window puts them back in the bag rather than leaving them in a table anyone can open.
    std::array<std::optional<storage::StoredItemStack>, kGridCells> grid;
    std::optional<OpenWindow> open;
    /// What was last said about the open window, so a turn that changed nothing says nothing. Both
    /// are cleared when a window opens, because the next one is a different window.
    std::vector<std::optional<storage::StoredItemStack>> saidSlots;
    /// What was last said about the open furnace, so a turn that changed nothing says nothing.
    std::optional<FurnaceNumbers> saidNumbers;
    /// Turns since the window's contents were last read out of the world. Reading a double chest
    /// means parsing two lists of stacks, which is not a thing to do on every turn of the tick
    /// loop: the numbers are cheap and go every turn, the contents go a few times a second.
    std::int32_t sinceRead = 0;
    /// The last window number handed out, so a freshly opened one never reuses the previous. One
    /// byte on the wire, and zero is the player's own window, so it runs from one.
    std::int32_t nextContainerId = 0;
};

/// How high the window numbers climb before starting again. parity: `ClientSession`'s own ceiling
/// on the Java side, which is the same hundred.
inline constexpr std::int32_t kMaxContainerId = 100;

/// What clicking this block opens, or nothing when it opens no window this build draws.
[[nodiscard]] std::optional<OpenWindow> windowFor(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                  std::int32_t z);

/// Which of the block's own slots a window's container and slot name, or nothing when the pair
/// means nothing in this window: a chest numbers its slots straight through, and a furnace gives
/// each of its three a container of its own.
[[nodiscard]] std::optional<std::int32_t> blockSlotOf(const OpenWindow& window, std::int32_t container,
                                                      std::int32_t slot);

/// What a furnace window has to say about itself, or nothing when the block is not one.
[[nodiscard]] std::optional<FurnaceNumbers> furnaceNumbers(world::LoadedWorld& world, const OpenWindow& window);

/// What the window holds, in its own slot order.
[[nodiscard]] std::vector<std::optional<storage::StoredItemStack>>
readWindow(world::LoadedWorld& world, const OpenWindow& window, const player::PlayerSession* owner);

[[nodiscard]] std::optional<storage::StoredItemStack> readWindowSlot(world::LoadedWorld& world,
                                                                     const OpenWindow& window, std::int32_t slot,
                                                                     const player::PlayerSession* owner);

/// Writes one slot back into the block. False when the block is not there any more, which is what
/// stops a window surviving its own chest.
bool writeWindowSlot(world::LoadedWorld& world, const OpenWindow& window, std::int32_t slot,
                     const std::optional<storage::StoredItemStack>& stack, player::PlayerSession* owner);

} // namespace basalt::server::runtime::bedrock
