#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/bedrock/BedrockMaps.hpp>
#include <basalt/wire/bedrock/BedrockMapConversion.hpp>

namespace basalt::server::runtime::bedrock {

void decorateMapStack(wire::bedrock::BedrockItemStack& item, const storage::StoredItemStack& stack) {
    if (!MapSystem::isMap(stack.id)) {
        return;
    }
    if (const std::optional<std::int32_t> id = MapSystem::mapIdOf(stack.components); id.has_value()) {
        item.mapId = *id;
    }
    if (const std::int32_t data = wire::bedrock::bedrockMapExplorerData(stack.id); data != 0) {
        item.data = data;
    }
}

} // namespace basalt::server::runtime::bedrock
