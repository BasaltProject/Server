#pragma once

#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    /// How many slots the window shows, both halves counted.
    std::int32_t size = 0;
    /// The block's own path, because what a container keeps its stacks under is per block.
    std::string block;
};

/// What one Bedrock client is holding outside its slots, and which block it has open.
///
/// Shared rather than owned by the player, because the world thread is what moves both and a call
/// already on its way there outlives the player it was made for.
struct BedrockWindows {
    /// What is being dragged. That edition keeps it in a window of its own rather than outside the
    /// windows, but it is still one stack that belongs to nobody until it is put down.
    std::optional<storage::StoredItemStack> cursor;
    std::optional<OpenWindow> open;
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

/// What the window holds, in its own slot order.
[[nodiscard]] std::vector<std::optional<storage::StoredItemStack>> readWindow(world::LoadedWorld& world,
                                                                              const OpenWindow& window);

[[nodiscard]] std::optional<storage::StoredItemStack> readWindowSlot(world::LoadedWorld& world,
                                                                     const OpenWindow& window, std::int32_t slot);

/// Writes one slot back into the block. False when the block is not there any more, which is what
/// stops a window surviving its own chest.
bool writeWindowSlot(world::LoadedWorld& world, const OpenWindow& window, std::int32_t slot,
                     const std::optional<storage::StoredItemStack>& stack);

} // namespace basalt::server::runtime::bedrock
