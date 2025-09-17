#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/world/ChunkTypes.hpp>

#include <stdexcept>
#include <utility>

namespace basalt::server::world {
namespace {

using wire::common::Identifier;

/// The block entity type a block owns, or the name itself when it already is one.
[[nodiscard]] Identifier typeOf(const Identifier& given) {
    const Identifier* owned = registry::blockEntityTypes::ofBlock(given);
    return owned == nullptr ? given : *owned;
}

} // namespace

ChunkBlockEntity::ChunkBlockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ, const Identifier& type,
                                   wire::nbt::NbtCompound data)
    : localX_(localX), y_(y), localZ_(localZ), type_(typeOf(type)), data_(std::move(data)) {
    if (!isInsideChunk(localX, y, localZ, kOverworldHeight)) {
        throw std::out_of_range{"block entity outside the chunk"};
    }
}

ChunkBlockEntity ChunkBlockEntity::withData(wire::nbt::NbtCompound data) const {
    return ChunkBlockEntity{localX_, y_, localZ_, type_, std::move(data)};
}

bool isInsideChunk(std::int32_t localX, std::int32_t y, std::int32_t localZ, ChunkHeight bounds) {
    return localX >= 0 && localX <= 15 && localZ >= 0 && localZ <= 15 && y >= bounds.minY() && y <= bounds.maxY();
}

} // namespace basalt::server::world
