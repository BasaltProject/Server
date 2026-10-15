#pragma once

#include <basalt/server/runtime/bedrock/BedrockWindows.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace basalt::server::runtime::bedrock {

inline constexpr std::uint64_t kRuntimeEntityId = 1;

inline constexpr std::uint32_t kOffhandContainer = 119;
inline constexpr std::uint32_t kArmourContainer = 120;

[[nodiscard]] std::uint64_t runtimeIdFor(std::int32_t entityId);

[[nodiscard]] std::int64_t nowEpochSeconds();

[[nodiscard]] wire::bedrock::BedrockItemStack itemStackFor(const std::optional<storage::StoredItemStack>& stack,
                                                           int protocol);

void drawWindowSlots(const std::shared_ptr<BedrockMailbox>& mailbox, int protocol, const OpenWindow& window,
                     const std::vector<std::optional<storage::StoredItemStack>>& slots);

} // namespace basalt::server::runtime::bedrock
