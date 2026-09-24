#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockMaps.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>

#include <cstdint>

namespace basalt::server::runtime::bedrock {

/// A Java map update as the other edition draws it: colours become pixels, and every marker gets a
/// Bedrock icon and colour. What Geyser does, which is the only reference there is.
[[nodiscard]] wire::bedrock::BedrockMapData bedrockMapOf(const wire::protocol::play::MapItemDataPacket& update,
                                                         std::uint8_t dimension);

/// Puts which map a stack shows, and which explorer map it is, on a Bedrock stack.
void decorateMapStack(wire::bedrock::BedrockItemStack& item, const storage::StoredItemStack& stack);

} // namespace basalt::server::runtime::bedrock
