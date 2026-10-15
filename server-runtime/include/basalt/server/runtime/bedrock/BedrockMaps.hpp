#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockMaps.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>

#include <cstdint>

namespace basalt::server::runtime::bedrock {

[[nodiscard]] wire::bedrock::BedrockMapData bedrockMapOf(const wire::protocol::play::MapItemDataPacket& update,
                                                         std::uint8_t dimension);

void decorateMapStack(wire::bedrock::BedrockItemStack& item, const storage::StoredItemStack& stack);

} // namespace basalt::server::runtime::bedrock
