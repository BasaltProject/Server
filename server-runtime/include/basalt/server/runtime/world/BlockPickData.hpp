#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>

namespace basalt::server::world {
class ChunkBlockEntity;
}

namespace basalt::server::runtime::world {

namespace blockPickData {

void addBlockData(const server::world::ChunkBlockEntity& entity, wire::nbt::NbtCompound& components);

/// parity: the `getCloneItemStack` overrides that look at the block.
void addCloneComponents(std::int32_t stateId, const server::world::ChunkBlockEntity* entity, bool includeData,
                        wire::nbt::NbtCompound& components);

} // namespace blockPickData
} // namespace basalt::server::runtime::world
