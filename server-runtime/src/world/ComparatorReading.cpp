#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::comparatorReading {
namespace {

/// parity: `ShelfBlock.getAnalogOutputSignal`, which needs the shelf's own facing and its slots.
[[nodiscard]] std::optional<std::int32_t> shelfSignal(WorldState& world, const std::int32_t x, const std::int32_t y,
                                                      const std::int32_t z, const std::int32_t stateId,
                                                      const std::string_view from) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !shelves::isShelf(block->path())) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto facing = properties.find("facing");
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    return shelves::comparatorSignal(entity == nullptr ? nullptr : &entity->data(),
                                     facing == properties.end() ? std::string_view{} : facing->second, from);
}

} // namespace

std::optional<std::int32_t> at(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                               const std::string_view from) {
    const containers::Readers readers{
        .shelf = [&world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t stateId,
                          const std::string_view from) { return shelfSignal(world, x, y, z, stateId, from); },
        .crafter = [](const wire::nbt::NbtCompound& data) { return crafters::comparatorSignal(data); },
        .cart =
            [&world](const std::int32_t x, const std::int32_t y, const std::int32_t z) {
                return detectorRails::comparatorSignal(world, x, y, z, world.entities());
            }};
    return containers::analogSignal(world, x, y, z, from, readers);
}

bool exists(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    // parity: `ShelfBlock.hasAnalogOutputSignal` is true whichever side the comparator is on, even
    // though `getAnalogOutputSignal` only answers from behind.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block != nullptr && shelves::isShelf(block->path())) {
        return true;
    }
    return at(world, x, y, z, {}).has_value();
}

redstoneComponents::ContainerSignalAt of(WorldState& world) {
    return [&world](const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::string_view from) {
        return at(world, x, y, z, from);
    };
}

} // namespace basalt::server::runtime::world::comparatorReading
