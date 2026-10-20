#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>

#include <cstdint>

namespace basalt::server::runtime::bedrock {

void decorateMapStack(wire::bedrock::BedrockItemStack& item, const storage::StoredItemStack& stack);

} // namespace basalt::server::runtime::bedrock
