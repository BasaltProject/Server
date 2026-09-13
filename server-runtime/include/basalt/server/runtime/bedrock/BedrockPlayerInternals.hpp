#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::bedrock {

/// What the two halves of a Bedrock player share: `BedrockPlayer.cpp` has the lifecycle and the
/// world, `BedrockPlayerConnection.cpp` has everything this player can be told.

/// What `start_game` told this client it is. Every client is told the same number: a Bedrock
/// session only ever draws one player of its own.
inline constexpr std::uint64_t kRuntimeEntityId = 1;

/// The two windows a player has beside the one they carry. Neither number has moved in this window.
inline constexpr std::uint32_t kOffhandContainer = 119;
inline constexpr std::uint32_t kArmourContainer = 120;

/// Somebody else's number, as this client knows it. One is the client's own, from `start_game`,
/// and every entity id is one or more, so they are all moved out of the way by exactly that.
[[nodiscard]] std::uint64_t runtimeIdFor(std::int32_t entityId);

[[nodiscard]] std::int64_t nowEpochSeconds();

/// One stack as the other edition carries it, or air when it has no such item. Free rather than a
/// member because the answer only depends on the protocol, and what draws a window may outlive the
/// player it was drawn for.
[[nodiscard]] wire::bedrock::BedrockItemStack itemStackFor(const std::optional<storage::StoredItemStack>& stack,
                                                           int protocol);

} // namespace basalt::server::runtime::bedrock
